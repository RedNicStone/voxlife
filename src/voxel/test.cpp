#include "test.h"

#include <chrono>
#include <daxa/daxa.hpp>

#include <daxa/pipeline.hpp>
#include <daxa/types.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/imgui.hpp>
#include <iostream>

#include <daxa/utils/task_graph.hpp>

#include "shaders/shared.inl"
#include "voxel/test/player.hpp"

#include "test/application.h"
#include <thread>
#include "write_file.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vec_swizzle.hpp>

template <typename T>
void upload_vector_task(daxa::TaskGraph &tg, daxa::TaskBufferView task_buffer, std::vector<T> const *data) {
    tg.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_buffer),
        },
        .task = [=](daxa::TaskInterface ti) {
            auto size = sizeof(T) * data->size();
            auto staging_buffer_id = ti.device.create_buffer({
                .size = size,
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "my staging buffer",
            });
            ti.recorder.destroy_buffer_deferred(staging_buffer_id);
            auto *buffer_ptr = ti.device.buffer_host_address_as<T>(staging_buffer_id).value();
            memcpy(buffer_ptr, data->data(), size);
            daxa::TaskBufferAttachmentInfo const &buffer_at_info = ti.get(task_buffer);
            std::span<daxa::BufferId const> runtime_ids = buffer_at_info.ids;
            daxa::BufferId id = runtime_ids[0];
            ti.recorder.copy_buffer_to_buffer({
                .src_buffer = staging_buffer_id,
                .dst_buffer = id,
                .size = size,
            });
        },
        .name = "upload std::vector data",
    });
}

void upload_frame_constants_task(daxa::TaskGraph &tg, daxa::TaskBufferView frame_constants, FrameConstants *data) {
}


struct TextureManifest {
    daxa::ImageId image;
};
struct CpuModelManifest {
    daxa::BufferId voxel_buffer;

    glm::vec3 aabb_min{};
    glm::vec3 aabb_max{};
    uint32_t texture_id{};

    auto get_extent() const {
        return glm::round(aabb_max - aabb_min);
    }

    void finalize(daxa::Device &device) {
        glm::uvec3 volume_extent = glm::max(glm::vec3(1), glm::round(aabb_max - aabb_min));
        voxel_buffer = device.create_buffer({
            .size = volume_extent.x * volume_extent.y * volume_extent.z * sizeof(GpuVoxel),
            .name = "model voxels",
        });
    }
};
struct VoxelizeApp : Application {
    daxa::Instance instance;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::ImGuiRenderer imgui_renderer;
    daxa::PipelineManager pipeline_manager;

    std::shared_ptr<daxa::RasterPipeline> triangle_draw_pipe;
    std::shared_ptr<daxa::RasterPipeline> triangle_wire_draw_pipe;
    std::shared_ptr<daxa::RasterPipeline> box_draw_pipe;
    std::shared_ptr<daxa::RasterPipeline> voxelize_draw_pipe;
    std::shared_ptr<daxa::RasterPipeline> voxelize_draw_msaa_pipe;
    std::shared_ptr<daxa::ComputePipeline> voxelize_preprocess_pipe;
    std::shared_ptr<daxa::ComputePipeline> voxelize_test_pipe;

    daxa::TaskBuffer task_vertex_buffer;
    daxa::TaskImage task_textures;
    daxa::TaskBuffer task_model_manifests;
    daxa::TaskBuffer task_cube_indices;
    daxa::TaskBuffer task_frame_constants;
    daxa::TaskBuffer task_model_voxels;

    daxa::TaskImage task_swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};
    daxa::TaskImage task_depth_image = daxa::TaskImage{{.name = "depth image"}};

    daxa::TaskGraph loop_task_graph;

    daxa::SamplerId sampler_llr;
    daxa::SamplerId sampler_nnr;

    FrameConstants frame_constants;
    std::vector<MyVertex> vertices;
    std::vector<uint16_t> cube_indices;
    std::unordered_map<uint32_t, TextureManifest> texture_manifests;
    std::vector<CpuModelManifest> model_manifests;
};

void init(VoxelizeApp *self) {
    self->instance = daxa::create_instance({});
    self->device = self->instance.create_device_2(self->instance.choose_device({}, {.max_allowed_buffers = 100000}));

    self->sampler_llr = self->device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .address_mode_u = daxa::SamplerAddressMode::REPEAT,
        .address_mode_v = daxa::SamplerAddressMode::REPEAT,
        .address_mode_w = daxa::SamplerAddressMode::REPEAT,
        .name = "sampler llr",
    });
    self->sampler_nnr = self->device.create_sampler({
        .magnification_filter = daxa::Filter::NEAREST,
        .minification_filter = daxa::Filter::NEAREST,
        .address_mode_u = daxa::SamplerAddressMode::REPEAT,
        .address_mode_v = daxa::SamplerAddressMode::REPEAT,
        .address_mode_w = daxa::SamplerAddressMode::REPEAT,
        .name = "sampler nnr",
    });
}

void deinit(VoxelizeApp *self) {
    self->device.wait_idle();
    self->device.collect_garbage();

    for (auto const &[bsp_id, tex] : self->texture_manifests)
        self->device.destroy_image(tex.image);
    self->device.destroy_sampler(self->sampler_llr);
    self->device.destroy_sampler(self->sampler_nnr);
    for (auto const &model : self->model_manifests)
        self->device.destroy_buffer(model.voxel_buffer);
    self->device.destroy_buffer(self->task_model_manifests.get_state().buffers[0]);
    if (self->swapchain.is_valid()) {
        self->device.destroy_buffer(self->task_cube_indices.get_state().buffers[0]);
        self->device.destroy_image(self->task_depth_image.get_state().images[0]);
    }
    self->device.destroy_buffer(self->task_vertex_buffer.get_state().buffers[0]);
    self->device.destroy_buffer(self->task_frame_constants.get_state().buffers[0]);
}

void init_bsp_data(VoxelizeApp *self, voxlife::bsp::bsp_handle bsp_handle) {
    auto faces = voxlife::bsp::get_model_faces(bsp_handle, 0);
    self->vertices.clear();
    self->texture_manifests.clear();
    self->model_manifests.clear();

    bool aabb_set = false;
    uint32_t model_id = 0;

    auto to_voxel_space = [](glm::vec3 v) {
        return glm::vec3(v.x, v.y, v.z) * (0.0254f / 0.1f);
    };

    for (auto &face : faces) {
        auto texture_name = voxlife::bsp::get_texture_name(bsp_handle, face.texture_id);
        auto texture = voxlife::bsp::get_texture_data(bsp_handle, face.texture_id);

        if (texture_name == "SKY" || texture_name == "sky")
            continue;

        glm::vec3 face_aabb_min = glm::floor(to_voxel_space(face.vertices[0]));
        glm::vec3 face_aabb_max = glm::floor(to_voxel_space(face.vertices[0]) + 1.0f);
        for (auto const &v : face.vertices) {
            face_aabb_min = glm::min(face_aabb_min, glm::floor(to_voxel_space(v)));
            face_aabb_max = glm::max(face_aabb_max, glm::floor(to_voxel_space(v) + 1.0f));
        }

        if (self->model_manifests.empty()) {
            CpuModelManifest model;
            model.aabb_min = face_aabb_min;
            model.aabb_max = face_aabb_max;
            model.texture_id = face.texture_id;
            self->model_manifests.push_back(model);
        }
        auto *model = &self->model_manifests.back();

        auto new_aabb_min = glm::min(face_aabb_min, model->aabb_min);
        auto new_aabb_max = glm::max(face_aabb_max, model->aabb_max);

        bool has_new_texture = face.texture_id != model->texture_id;
        bool new_model_very_big = glm::any(glm::greaterThan(new_aabb_max - new_aabb_min, glm::vec3(250.0f)));
        bool would_double_size = glm::any(glm::greaterThan(new_aabb_max - new_aabb_min, 2.0f * (model->aabb_max - model->aabb_min)));
        bool curr_model_very_small = glm::any(glm::lessThan(model->aabb_max - model->aabb_min, glm::vec3(20.0f)));

        if (has_new_texture || new_model_very_big) {
            self->model_manifests.back().finalize(self->device);

            {
                self->model_manifests.push_back({});
                model = &self->model_manifests.back();
                model->aabb_min = face_aabb_min;
                model->aabb_max = face_aabb_max;
                model->texture_id = face.texture_id;
            }
        }
        model->aabb_min = glm::min(face_aabb_min, model->aabb_min);
        model->aabb_max = glm::max(face_aabb_max, model->aabb_max);

        auto tex = TextureManifest{};
        if (!self->texture_manifests.contains(face.texture_id)) {
            tex.image = self->device.create_image({
                .format = daxa::Format::R8G8B8A8_SRGB,
                .size = {texture.size.x, texture.size.y, 1},
                .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST,
                .name = "image",
            });
            self->texture_manifests[face.texture_id] = tex;
        } else {
            tex = self->texture_manifests[face.texture_id];
        }

        auto triangle_count = face.vertices.size() - 2;
        glm::vec3 v0, v1, v2;
        v0 = to_voxel_space(face.vertices[0]);
        v1 = to_voxel_space(face.vertices[1]);

        glm::vec2 uv0, uv1, uv2;
        uv0.x = (glm::dot(face.texture_coords.x.axis, face.vertices[0]) + face.texture_coords.x.shift) / float(texture.size.x);
        uv0.y = (glm::dot(face.texture_coords.y.axis, face.vertices[0]) + face.texture_coords.y.shift) / float(texture.size.y);
        uv1.x = (glm::dot(face.texture_coords.x.axis, face.vertices[1]) + face.texture_coords.x.shift) / float(texture.size.x);
        uv1.y = (glm::dot(face.texture_coords.y.axis, face.vertices[1]) + face.texture_coords.y.shift) / float(texture.size.y);

        for (int i = 0; i < triangle_count; ++i) {
            v2 = to_voxel_space(face.vertices[i + 2]);
            uv2.x = (glm::dot(face.texture_coords.x.axis, face.vertices[i + 2]) + face.texture_coords.x.shift) / float(texture.size.x);
            uv2.y = (glm::dot(face.texture_coords.y.axis, face.vertices[i + 2]) + face.texture_coords.y.shift) / float(texture.size.y);
            auto v = MyVertex{};
            v.tex_id = daxa_ImageViewIndex(tex.image.default_view().index);
            v.model_id = self->model_manifests.size() - 1;

            v.pos = {v0.x, v0.y, v0.z};
            v.uv = {uv0.x, uv0.y};
            self->vertices.push_back(v);

            v.pos = {v1.x, v1.y, v1.z};
            v.uv = {uv1.x, uv1.y};
            self->vertices.push_back(v);

            v.pos = {v2.x, v2.y, v2.z};
            v.uv = {uv2.x, uv2.y};
            self->vertices.push_back(v);

            v1 = v2;
            uv1 = uv2;
        }
    }
    self->model_manifests.back().finalize(self->device);

    auto model_manifests_buffer_id = self->device.create_buffer({
        .size = sizeof(GpuModelManifest) * self->model_manifests.size(),
        .name = "model manifests",
    });
    self->task_model_manifests = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&model_manifests_buffer_id, 1}},
        .name = "task model manifests",
    });

    self->task_model_voxels = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&self->model_manifests.back().voxel_buffer, 1}},
        .name = "task model voxels",
    });

    auto triangle_buffer_id = self->device.create_buffer({
        .size = self->vertices.size() * sizeof(MyVertex),
        .name = "my vertex data",
    });
    self->task_vertex_buffer = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&triangle_buffer_id, 1}},
        .name = "task vertex buffer",
    });

    auto frame_constants = FrameConstants{};
    frame_constants.sampler_llr = self->sampler_llr;

    auto frame_constants_buffer_id = self->device.create_buffer({
        .size = sizeof(FrameConstants),
        .name = "frame constants",
    });
    self->task_frame_constants = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&frame_constants_buffer_id, 1}},
        .name = "task frame constants",
    });

    self->task_textures = daxa::TaskImage{{.name = "textures"}};
    std::vector<daxa::ImageId> texture_images;
    for (auto [bsp_id, tex] : self->texture_manifests)
        texture_images.push_back(tex.image);
    self->task_textures.set_images({.images = texture_images});
}

void init_renderer(VoxelizeApp *self) {
    self->cube_indices.clear();
    self->init_window();

    self->swapchain = self->device.create_swapchain({
        .native_window = get_native_handle(self->window.glfw_ptr),
        .native_window_platform = get_native_platform(self->window.glfw_ptr),
        .present_mode = daxa::PresentMode::FIFO,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .name = "my swapchain",
    });

    self->imgui_renderer = daxa::ImGuiRenderer({
        .device = self->device,
        .format = self->swapchain.get_format(),
        .context = ImGui::GetCurrentContext(),
    });

    auto depth_image = self->device.create_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {self->window.size_x, self->window.size_y, 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
        .name = "depth image",
    });
    self->task_depth_image.set_images({.images = {&depth_image, 1}});
    self->cube_indices = std::vector<uint16_t>{0, 1, 2, 3, 4, 5, 6, 1};
    auto cube_indices_buffer_id = self->device.create_buffer({
        .size = sizeof(uint16_t) * self->cube_indices.size(),
        .name = "cube_indices",
    });
    self->task_cube_indices = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&cube_indices_buffer_id, 1}},
        .name = "task cube indices",
    });
}

void init_pipelines(VoxelizeApp *self) {
    // if (self->pipeline_manager.is_valid())
    //     return;

    self->pipeline_manager = daxa::PipelineManager({
        .device = self->device,
        .shader_compile_options = {
            .root_paths = {
                DAXA_SHADER_INCLUDE_DIR,
                "../../src/voxel/shaders",
            },
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .register_null_pipelines_when_first_compile_fails = true,
        .name = "my pipeline manager",
    });

    if (self->swapchain.is_valid()) {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .color_attachments = {{
                .format = self->swapchain.get_format(),
                .blend = daxa::BlendInfo{
                    .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA,
                    .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA,
                    .src_alpha_blend_factor = daxa::BlendFactor::ONE,
                    .dst_alpha_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA,
                },
            }},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
            },
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST,
                .polygon_mode = daxa::PolygonMode::FILL,
            },
            .push_constant_size = sizeof(TriDrawPush),
        });
        if (!result.value()->is_valid())
            std::cerr << result.message() << std::endl;
        self->triangle_draw_pipe = result.value();

        result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .color_attachments = {{
                .format = self->swapchain.get_format(),
                .blend = daxa::BlendInfo{
                    .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA,
                    .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA,
                    .src_alpha_blend_factor = daxa::BlendFactor::ONE,
                    .dst_alpha_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA,
                },
            }},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
            },
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST,
                .polygon_mode = daxa::PolygonMode::LINE,
            },
            .push_constant_size = sizeof(TriDrawPush),
        });
        if (!result.value()->is_valid())
            std::cerr << result.message() << std::endl;
        self->triangle_wire_draw_pipe = result.value();

        result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"box_draw.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"box_draw.glsl"}},
            .color_attachments = {{
                .format = self->swapchain.get_format(),
                .blend = daxa::BlendInfo{
                    .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA,
                    .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA,
                    .src_alpha_blend_factor = daxa::BlendFactor::ONE,
                    .dst_alpha_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA,
                },
            }},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
            },
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::TRIANGLE_FAN,
                .polygon_mode = daxa::PolygonMode::FILL,
            },
            .push_constant_size = sizeof(BoxDrawPush),
        });
        if (!result.value()->is_valid())
            std::cerr << result.message() << std::endl;
        self->box_draw_pipe = result.value();
    }
    {
        auto result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxelize.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxelize.glsl"}},
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST,
                .polygon_mode = daxa::PolygonMode::FILL,
            },
            .push_constant_size = sizeof(VoxelizeDrawPush),
        });
        if (!result.value()->is_valid())
            std::cerr << result.message() << std::endl;
        self->voxelize_draw_pipe = result.value();

        result = self->pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxelize.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxelize.glsl"}},
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST,
                .polygon_mode = daxa::PolygonMode::FILL,
                .static_state_sample_count = daxa::RasterizationSamples::E8,
            },
            .push_constant_size = sizeof(VoxelizeDrawPush),
        });
        if (!result.value()->is_valid())
            std::cerr << result.message() << std::endl;
        self->voxelize_draw_msaa_pipe = result.value();
    }
    {
        auto result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = {.source = daxa::ShaderFile{"voxelize.glsl"}},
            .push_constant_size = sizeof(VoxelizeDrawPush),
        });
        if (!result.value()->is_valid())
            std::cerr << result.message() << std::endl;
        self->voxelize_preprocess_pipe = result.value();

        result = self->pipeline_manager.add_compute_pipeline({
            .shader_info = {.source = daxa::ShaderFile{"voxelize.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"TEST"}}}},
            .push_constant_size = sizeof(VoxelizeDrawPush),
        });
        if (!result.value()->is_valid())
            std::cerr << result.message() << std::endl;
        self->voxelize_test_pipe = result.value();
    }

    while (!self->pipeline_manager.all_pipelines_valid()) {
        auto reload_result = self->pipeline_manager.reload_all();
        if (auto *reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
            std::cout << reload_err->message << std::endl;
        }
    }
    std::cout << "Compiled all pipelines." << std::endl;
}

void record_frame(VoxelizeApp *self) {
    auto &task_graph = self->loop_task_graph;

    task_graph = daxa::TaskGraph({
        .device = self->device,
        .swapchain = self->swapchain,
        .name = "loop",
    });

    task_graph.use_persistent_buffer(self->task_vertex_buffer);
    task_graph.use_persistent_image(self->task_textures);
    task_graph.use_persistent_buffer(self->task_model_manifests);
    task_graph.use_persistent_buffer(self->task_frame_constants);
    task_graph.use_persistent_buffer(self->task_model_voxels);
    if (self->swapchain.is_valid()) {
        task_graph.use_persistent_image(self->task_swapchain_image);
        task_graph.use_persistent_image(self->task_depth_image);
        task_graph.use_persistent_buffer(self->task_cube_indices);
    }

    task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, self->task_frame_constants),
        },
        .task = [=](daxa::TaskInterface ti) {
            auto staging_buffer_id = ti.device.create_buffer({
                .size = sizeof(FrameConstants),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "my staging buffer",
            });
            ti.recorder.destroy_buffer_deferred(staging_buffer_id);
            auto *buffer_ptr = ti.device.buffer_host_address_as<FrameConstants>(staging_buffer_id).value();
            *buffer_ptr = self->frame_constants;
            daxa::TaskBufferAttachmentInfo const &buffer_at_info = ti.get(self->task_frame_constants);
            std::span<daxa::BufferId const> runtime_ids = buffer_at_info.ids;
            daxa::BufferId id = runtime_ids[0];
            ti.recorder.copy_buffer_to_buffer({
                .src_buffer = staging_buffer_id,
                .dst_buffer = id,
                .size = sizeof(FrameConstants),
            });
        },
        .name = "upload frame constants",
    });

    auto task_processed_vertex_buffer = task_graph.create_transient_buffer({
        .size = uint32_t(self->vertices.size() * sizeof(MyVertex)),
        .name = "processed vertices",
    });

    task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, self->task_frame_constants),
            daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, self->task_vertex_buffer),
            daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, self->task_model_manifests),
            daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_WRITE, task_processed_vertex_buffer),
        },
        .task = [=](daxa::TaskInterface ti) {
            ti.recorder.set_pipeline(*self->voxelize_preprocess_pipe);
            ti.recorder.push_constant(VoxelizeDrawPush{
                .frame_constants = ti.device.buffer_device_address(ti.get(self->task_frame_constants.view()).ids[0]).value(),
                .model_manifests = ti.device.buffer_device_address(ti.get(self->task_model_manifests.view()).ids[0]).value(),
                .vertices = ti.device.buffer_device_address(ti.get(self->task_vertex_buffer.view()).ids[0]).value(),
                .processed_vertices = ti.device.buffer_device_address(ti.get(task_processed_vertex_buffer).ids[0]).value(),
                .triangle_count = uint32_t(self->vertices.size() / 3),
            });
            ti.recorder.dispatch({uint32_t(self->vertices.size() / 3 + 127) / 128, 1, 1});
        },
        .name = "preprocess verts",
    });

    task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, self->task_model_voxels),
        },
        .task = [=](daxa::TaskInterface ti) {
            for (auto const &model : self->model_manifests) {
                auto buffer_size = ti.device.buffer_info(model.voxel_buffer).value().size;
                ti.recorder.clear_buffer({.buffer = model.voxel_buffer, .size = buffer_size});
            }
        },
        .name = "clear voxel volumes",
    });

    task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ, self->task_frame_constants),
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ, self->task_model_manifests),
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ, task_processed_vertex_buffer),
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ_WRITE, self->task_model_voxels),
            daxa::inl_attachment(daxa::TaskImageAccess::FRAGMENT_SHADER_SAMPLED, self->task_textures),
        },
        .task = [=](daxa::TaskInterface ti) {
            daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
                .render_area = {.width = 256, .height = 256},
            });
            render_recorder.set_pipeline(self->settings.use_msaa ? *self->voxelize_draw_msaa_pipe : *self->voxelize_draw_pipe);
            render_recorder.push_constant(VoxelizeDrawPush{
                .frame_constants = ti.device.buffer_device_address(ti.get(self->task_frame_constants.view()).ids[0]).value(),
                .model_manifests = ti.device.buffer_device_address(ti.get(self->task_model_manifests.view()).ids[0]).value(),
                .processed_vertices = ti.device.buffer_device_address(ti.get(task_processed_vertex_buffer).ids[0]).value(),
                .triangle_count = uint32_t(self->vertices.size() / 3),
            });
            render_recorder.draw({.vertex_count = uint32_t(self->vertices.size())});
            ti.recorder = std::move(render_recorder).end_renderpass();
        },
        .name = "voxelize",
    });

    if (self->swapchain.is_valid()) {
        task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, self->task_swapchain_image),
                daxa::inl_attachment(daxa::TaskImageAccess::DEPTH_ATTACHMENT, self->task_depth_image),
                daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, self->task_frame_constants),

                daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, self->task_vertex_buffer),
                daxa::inl_attachment(daxa::TaskImageAccess::FRAGMENT_SHADER_SAMPLED, self->task_textures),

                daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, self->task_model_manifests),
                daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, self->task_model_voxels),
                daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ, self->task_cube_indices),
            },
            .task = [=](daxa::TaskInterface ti) {
                auto const &image_attach_info = ti.get(self->task_swapchain_image);
                auto const &depth_attach_info = ti.get(self->task_depth_image);
                auto image_info = ti.device.image_info(image_attach_info.ids[0]).value();
                daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
                    .color_attachments = std::array{
                        daxa::RenderAttachmentInfo{
                            .image_view = image_attach_info.view_ids[0],
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<daxa::f32, 4>{0.1f, 0.1f, 0.1f, 1.0f},
                        },
                    },
                    .depth_attachment = daxa::RenderAttachmentInfo{
                        .image_view = depth_attach_info.view_ids[0],
                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                        .clear_value = daxa::DepthValue{1.0f},
                    },
                    .render_area = {.width = image_info.size.x, .height = image_info.size.y},
                });

                if (self->settings.show_mesh) {
                    render_recorder.set_pipeline(*self->triangle_draw_pipe);
                    render_recorder.push_constant(TriDrawPush{
                        .frame_constants = ti.device.buffer_device_address(ti.get(self->task_frame_constants.view()).ids[0]).value(),
                        .vertices = ti.device.buffer_device_address(ti.get(self->task_vertex_buffer.view()).ids[0]).value(),
                        .overlay = {0, 0, 0, 0},
                    });
                    render_recorder.draw({.vertex_count = uint32_t(self->vertices.size())});
                }

                if (self->settings.show_wireframe) {
                    render_recorder.set_pipeline(*self->triangle_wire_draw_pipe);
                    render_recorder.push_constant(TriDrawPush{
                        .frame_constants = ti.device.buffer_device_address(ti.get(self->task_frame_constants.view()).ids[0]).value(),
                        .vertices = ti.device.buffer_device_address(ti.get(self->task_vertex_buffer.view()).ids[0]).value(),
                        .overlay = {1, 1, 1, 0.1f},
                    });
                    render_recorder.draw({.vertex_count = uint32_t(self->vertices.size())});
                }

                if (self->settings.show_voxels) {
                    render_recorder.set_pipeline(*self->box_draw_pipe);
                    render_recorder.push_constant(BoxDrawPush{
                        .frame_constants = ti.device.buffer_device_address(ti.get(self->task_frame_constants.view()).ids[0]).value(),
                        .model_manifests = ti.device.buffer_device_address(ti.get(self->task_model_manifests.view()).ids[0]).value(),
                    });
                    render_recorder.set_index_buffer({
                        .id = ti.get(self->task_cube_indices).ids[0],
                        .index_type = daxa::IndexType::uint16,
                    });
                    render_recorder.draw_indexed({.index_count = 8, .instance_count = uint32_t(self->model_manifests.size())});
                }

                ti.recorder = std::move(render_recorder).end_renderpass();
            },
            .name = "draw",
        });

        task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, self->task_swapchain_image),
            },
            .task = [=](daxa::TaskInterface const &ti) {
                self->imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, ti.get(self->task_swapchain_image).ids[0], self->window.size_x, self->window.size_y);
            },
            .name = "ImGui Task",
        });
    }

    task_graph.submit({});
    if (self->swapchain.is_valid())
        task_graph.present({});
    task_graph.complete({});
}

auto t0 = Clock::now();

void upload_data(VoxelizeApp *self, voxlife::bsp::bsp_handle bsp_handle) {
    auto task_graph = daxa::TaskGraph({
        .device = self->device,
        .name = "upload",
    });
    task_graph.use_persistent_buffer(self->task_vertex_buffer);
    task_graph.use_persistent_image(self->task_textures);
    task_graph.use_persistent_buffer(self->task_model_manifests);
    upload_vector_task(task_graph, self->task_vertex_buffer, &self->vertices);
    if (self->swapchain.is_valid()) {
        task_graph.use_persistent_buffer(self->task_cube_indices);
        upload_vector_task(task_graph, self->task_cube_indices, &self->cube_indices);
    }

    std::vector<GpuModelManifest> gpu_model_manifests;
    gpu_model_manifests.reserve(self->model_manifests.size());
    for (auto const &model : self->model_manifests) {
        gpu_model_manifests.push_back({
            .voxels = self->device.buffer_device_address(model.voxel_buffer).value(),
            .aabb_min = std::bit_cast<daxa_f32vec3>(model.aabb_min),
            .aabb_max = std::bit_cast<daxa_f32vec3>(model.aabb_max),
        });
    }
    upload_vector_task(task_graph, self->task_model_manifests, &gpu_model_manifests);

    task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, self->task_textures),
        },
        .task = [=](daxa::TaskInterface ti) {
            for (auto [bsp_id, tex] : self->texture_manifests) {
                auto bsp_tex = voxlife::bsp::get_texture_data(bsp_handle, bsp_id);
                auto size = size_t(bsp_tex.size.x * bsp_tex.size.y * sizeof(uint32_t));
                auto staging_buffer_id = ti.device.create_buffer({
                    .size = size,
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "my staging buffer",
                });
                ti.recorder.destroy_buffer_deferred(staging_buffer_id);
                auto *buffer_ptr = ti.device.buffer_host_address_as<unsigned char>(staging_buffer_id).value();
                for (uint32_t i = 0; i < bsp_tex.size.x * bsp_tex.size.y; ++i) {
                    buffer_ptr[i * 4 + 0] = bsp_tex.data[i].r;
                    buffer_ptr[i * 4 + 1] = bsp_tex.data[i].g;
                    buffer_ptr[i * 4 + 2] = bsp_tex.data[i].b;
                    buffer_ptr[i * 4 + 3] = 255;
                }
                ti.recorder.copy_buffer_to_image({
                    .buffer = staging_buffer_id,
                    .image = tex.image,
                    .image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                    .image_offset = {0, 0, 0},
                    .image_extent = {bsp_tex.size.x, bsp_tex.size.y, 1},
                });
            }
        },
        .name = "upload textures",
    });

    task_graph.submit({});
    task_graph.complete({});
    task_graph.execute({});
}

void download_data(VoxelizeApp *self, std::string_view level_name, std::vector<struct Model> &models) {
    auto task_graph = daxa::TaskGraph({
        .device = self->device,
        .name = "download",
    });
    task_graph.use_persistent_buffer(self->task_model_voxels);

    auto model_buffers = std::vector<daxa::BufferId>{};
    model_buffers.reserve(self->model_manifests.size());
    models.reserve(self->model_manifests.size());

    auto voxel_models = std::unordered_map<uint32_t, std::vector<VoxelModel>>{};

    task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, self->task_model_voxels),
        },
        .task = [&](daxa::TaskInterface ti) {
            for (auto const &model : self->model_manifests) {
                auto buffer_size = ti.device.buffer_info(model.voxel_buffer).value().size;
                auto staging_buffer_id = ti.device.create_buffer({
                    .size = buffer_size,
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "my staging buffer",
                });
                model_buffers.push_back(staging_buffer_id);
                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = model.voxel_buffer,
                    .dst_buffer = staging_buffer_id,
                    .size = buffer_size,
                });

                auto voxel_model = VoxelModel{
                    .voxels = std::span(ti.device.buffer_host_address_as<Voxel>(staging_buffer_id).value(), buffer_size / sizeof(Voxel)),
                    .pos = glm::i32vec3(glm::floor((model.aabb_min + model.aabb_max) * 0.5f)),
                    .size = model.get_extent(),
                };
                if (all(lessThanEqual(voxel_model.size, glm::uvec3(256))))
                    voxel_models[model.texture_id].push_back(voxel_model);
            }
        },
        .name = "download data",
    });

    task_graph.submit({});
    task_graph.complete({});
    task_graph.execute({});

    self->device.wait_idle();

    std::filesystem::create_directories(std::format("brush/{}", level_name));

    for (int model_index = 0; model_index < voxel_models.size(); ++model_index) {
        auto it = voxel_models.begin();
        std::advance(it, model_index);
        auto const &[texture_id, voxel_model_list] = *it;
        write_magicavoxel_model(std::format("brush/{}/{}.vox", level_name, model_index), std::span(voxel_model_list));

        models.emplace_back();
        auto &out_model = models.back();
        out_model.name = std::format("{}", model_index);
        out_model.size = {};
        out_model.pos = {};
    }

    for (auto &buffer : model_buffers)
        self->device.destroy_buffer(buffer);
}

void update(VoxelizeApp *self) {
    auto recreate_render_image = [self](daxa::TaskImage &task_image_id) {
        auto image_id = task_image_id.get_state().images[0];
        auto image_info = self->device.image_info(image_id).value();
        self->device.destroy_image(image_id);
        image_info.size = {self->window.size_x, self->window.size_y, 1},
        image_id = self->device.create_image(image_info);
        task_image_id.set_images({.images = {&image_id, 1}});
    };

    if (self->swapchain.is_valid()) {
        if (self->window.swapchain_out_of_date) {
            recreate_render_image(self->task_depth_image);
            self->swapchain.resize();
            self->window.swapchain_out_of_date = false;
        }
        auto swapchain_image = self->swapchain.acquire_next_image();
        if (swapchain_image.is_empty())
            return;
        self->task_swapchain_image.set_images({.images = std::span{&swapchain_image, 1}});
    }

    self->pipeline_manager.reload_all();

    auto reload_result = self->pipeline_manager.reload_all();
    if (auto *reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
        std::cout << reload_err->message << std::endl;
    } else if (auto *reload_success = daxa::get_if<daxa::PipelineReloadSuccess>(&reload_result)) {
        std::cout << "SUCCESS!" << std::endl;
    }

    self->frame_constants.world_to_view = std::bit_cast<daxa_f32mat4x4>(self->player.camera.vrot_mat * self->player.camera.vtrn_mat);
    self->frame_constants.view_to_world = std::bit_cast<daxa_f32mat4x4>(glm::inverse(self->player.camera.vrot_mat * self->player.camera.vtrn_mat));
    self->frame_constants.view_to_clip = std::bit_cast<daxa_f32mat4x4>(self->player.camera.proj_mat);
    self->frame_constants.clip_to_view = std::bit_cast<daxa_f32mat4x4>(glm::inverse(self->player.camera.proj_mat));
    auto t1 = Clock::now();
    self->frame_constants.time = std::chrono::duration<float>(t1 - t0).count();
    self->frame_constants.sampler_llr = self->settings.use_nearest ? self->sampler_nnr : self->sampler_llr;

    self->loop_task_graph.execute({});
    self->device.collect_garbage();

    // using namespace std::chrono_literals;
    // std::this_thread::sleep_for(50ms);
}

void voxelization_gui(voxlife::bsp::bsp_handle bsp_handle) {
    auto app = VoxelizeApp();
    init(&app);
    init_bsp_data(&app, bsp_handle);
    init_renderer(&app);
    init_pipelines(&app);
    upload_data(&app, bsp_handle);
    record_frame(&app);

    while (true) {
        if (!app.update())
            break;

        update(&app);
    }

    deinit(&app);
}

void voxelize_gpu(voxlife::bsp::bsp_handle bsp_handle, std::string_view level_name, std::vector<struct Model> &models) {
    static auto app = VoxelizeApp();
    init(&app);
    init_bsp_data(&app, bsp_handle);
    init_pipelines(&app);
    upload_data(&app, bsp_handle);
    record_frame(&app);
    update(&app);
    download_data(&app, level_name, models);
    deinit(&app);
}
