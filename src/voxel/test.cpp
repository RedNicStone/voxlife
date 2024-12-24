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

template <typename T>
void upload_vector_task(daxa::TaskGraph& tg, daxa::TaskBufferView task_buffer, std::vector<T> const* data) {
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
            auto* buffer_ptr = ti.device.buffer_host_address_as<T>(staging_buffer_id).value();
            memcpy(buffer_ptr, data->data(), size);
            daxa::TaskBufferAttachmentInfo const& buffer_at_info = ti.get(task_buffer);
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

void upload_frame_constants_task(daxa::TaskGraph& tg, daxa::TaskBufferView frame_constants, FrameConstants* data) {
    tg.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, frame_constants),
        },
        .task = [=](daxa::TaskInterface ti) {
            auto staging_buffer_id = ti.device.create_buffer({
                .size = sizeof(FrameConstants),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "my staging buffer",
            });
            ti.recorder.destroy_buffer_deferred(staging_buffer_id);
            auto* buffer_ptr = ti.device.buffer_host_address_as<FrameConstants>(staging_buffer_id).value();
            *buffer_ptr = *data;
            daxa::TaskBufferAttachmentInfo const& buffer_at_info = ti.get(frame_constants);
            std::span<daxa::BufferId const> runtime_ids = buffer_at_info.ids;
            daxa::BufferId id = runtime_ids[0];
            ti.recorder.copy_buffer_to_buffer({
                .src_buffer = staging_buffer_id,
                .dst_buffer = id,
                .size = sizeof(FrameConstants),
            });
        },
        .name = "upload vertices",
    });
}

void test_voxelization_gui(voxlife::bsp::bsp_handle bsp_handle) {
    auto app = Application();

    daxa::Instance instance = daxa::create_instance({});
    daxa::Device device = instance.create_device_2(instance.choose_device({}, {}));

    daxa::Swapchain swapchain = device.create_swapchain({
        .native_window = get_native_handle(app.window.glfw_ptr),
        .native_window_platform = get_native_platform(app.window.glfw_ptr),
        .present_mode = daxa::PresentMode::FIFO,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .name = "my swapchain",
    });

    auto imgui_renderer = daxa::ImGuiRenderer({
        .device = device,
        .format = swapchain.get_format(),
        .context = ImGui::GetCurrentContext(),
    });

    auto pipeline_manager = daxa::PipelineManager({
        .device = device,
        .shader_compile_options = {
            .root_paths = {
                DAXA_SHADER_INCLUDE_DIR,
                "src/voxel/shaders",
            },
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .register_null_pipelines_when_first_compile_fails = true,
        .name = "my pipeline manager",
    });
    std::shared_ptr<daxa::RasterPipeline> triangle_draw_pipe;
    std::shared_ptr<daxa::RasterPipeline> triangle_wire_draw_pipe;
    std::shared_ptr<daxa::RasterPipeline> box_draw_pipe;
    std::shared_ptr<daxa::RasterPipeline> voxelize_draw_pipe;
    std::shared_ptr<daxa::ComputePipeline> voxelize_preprocess_pipe;
    std::shared_ptr<daxa::ComputePipeline> voxelize_test_pipe;
    {
        auto result = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .color_attachments = {{
                .format = swapchain.get_format(),
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
        triangle_draw_pipe = result.value();

        result = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .color_attachments = {{
                .format = swapchain.get_format(),
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
        triangle_wire_draw_pipe = result.value();

        result = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"box_draw.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"box_draw.glsl"}},
            .color_attachments = {{
                .format = swapchain.get_format(),
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
        box_draw_pipe = result.value();

        result = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxelize.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"voxelize.glsl"}},
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST,
                .polygon_mode = daxa::PolygonMode::FILL,
                // .static_state_sample_count = daxa::RasterizationSamples::E8,
            },
            .push_constant_size = sizeof(VoxelizeDrawPush),
        });
        if (!result.value()->is_valid())
            std::cerr << result.message() << std::endl;
        voxelize_draw_pipe = result.value();

        {
            auto result = pipeline_manager.add_compute_pipeline({
                .shader_info = {.source = daxa::ShaderFile{"voxelize.glsl"}},
                .push_constant_size = sizeof(VoxelizeDrawPush),
            });
            if (!result.value()->is_valid())
                std::cerr << result.message() << std::endl;
            voxelize_preprocess_pipe = result.value();

            result = pipeline_manager.add_compute_pipeline({
                .shader_info = {.source = daxa::ShaderFile{"voxelize.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"TEST"}}}},
                .push_constant_size = sizeof(VoxelizeDrawPush),
            });
            if (!result.value()->is_valid())
                std::cerr << result.message() << std::endl;
            voxelize_test_pipe = result.value();
        }
    }

    while (!pipeline_manager.all_pipelines_valid()) {
        auto reload_result = pipeline_manager.reload_all();
        if (auto* reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
            std::cout << reload_err->message << std::endl;
        }
    }
    std::cout << "Compiled all pipelines." << std::endl;

    auto sampler_llr = device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .address_mode_u = daxa::SamplerAddressMode::REPEAT,
        .address_mode_v = daxa::SamplerAddressMode::REPEAT,
        .address_mode_w = daxa::SamplerAddressMode::REPEAT,
        .name = "sampler llr",
    });

    auto task_swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};
    auto task_depth_image = daxa::TaskImage{{.name = "depth image"}};

    auto depth_image = device.create_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {app.window.size_x, app.window.size_y, 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
        .name = "depth image",
    });
    task_depth_image.set_images({.images = {&depth_image, 1}});

    auto recreate_render_image = [&](daxa::ImageId& image_id, daxa::TaskImage& task_image_id) {
        auto image_info = device.info(image_id).value();
        device.destroy_image(image_id);
        image_info.size = {app.window.size_x, app.window.size_y, 1},
        image_id = device.create_image(image_info);
        task_image_id.set_images({.images = {&image_id, 1}});
    };

    auto vertices = std::vector<MyVertex>{};

    struct TextureManifest {
        daxa::ImageId image;
    };
    auto texture_manifests = std::unordered_map<uint32_t, TextureManifest>{};

    auto faces = voxlife::bsp::get_model_faces(bsp_handle, 0);
    uint32_t model_id = 0;
    uint32_t prev_texture_id = -1;

    bool aabb_set = false;

    struct CpuModelManifest {
        daxa::BufferId voxel_buffer;

        glm::vec3 aabb_min{};
        glm::vec3 aabb_max{};

        void finalize(daxa::Device& device) {
            glm::uvec3 volume_extent = glm::round(aabb_max - aabb_min);
            voxel_buffer = device.create_buffer({
                .size = volume_extent.x * volume_extent.y * volume_extent.z * sizeof(Voxel),
                .name = "model voxels",
            });
        }
    };
    std::vector<CpuModelManifest> model_manifests;

    auto to_voxel_space = [](glm::vec3 v) {
        return glm::vec3(-v.x, v.y, v.z) * (0.0254f / 0.1f);
    };

    for (auto& face : faces) {
        auto texture_name = voxlife::bsp::get_texture_name(bsp_handle, face.texture_id);
        auto texture = voxlife::bsp::get_texture_data(bsp_handle, face.texture_id);

        if (texture_name == "SKY" || texture_name == "sky")
            continue;

        glm::vec3 face_aabb_min = glm::floor(to_voxel_space(face.vertices[0]));
        glm::vec3 face_aabb_max = glm::ceil(to_voxel_space(face.vertices[0]));
        for (auto const& v : face.vertices) {
            face_aabb_min = glm::min(face_aabb_min, glm::floor(to_voxel_space(v)));
            face_aabb_max = glm::max(face_aabb_max, glm::ceil(to_voxel_space(v)));
        }

        if (model_manifests.empty()) {
            CpuModelManifest model;
            model.aabb_min = face_aabb_min;
            model.aabb_max = face_aabb_max;
            model_manifests.push_back(model);
        }
        auto* model = &model_manifests.back();

        auto new_aabb_min = glm::min(face_aabb_min, model->aabb_min);
        auto new_aabb_max = glm::max(face_aabb_max, model->aabb_max);

        bool has_new_texture = face.texture_id != prev_texture_id;
        bool new_model_very_big = glm::any(glm::greaterThan(new_aabb_max - new_aabb_min, glm::vec3(250.0f)));
        bool would_double_size = glm::any(glm::greaterThan(new_aabb_max - new_aabb_min, 2.0f * (model->aabb_max - model->aabb_min)));
        bool curr_model_very_small = glm::any(glm::lessThan(model->aabb_max - model->aabb_min, glm::vec3(20.0f)));

        if (new_model_very_big || (has_new_texture && !curr_model_very_small)) {
            model_manifests.back().finalize(device);

            {
                model_manifests.push_back({});
                model = &model_manifests.back();
                model->aabb_min = face_aabb_min;
                model->aabb_max = face_aabb_max;
                prev_texture_id = face.texture_id;
            }
        }
        model->aabb_min = glm::min(face_aabb_min, model->aabb_min);
        model->aabb_max = glm::max(face_aabb_max, model->aabb_max);

        auto tex = TextureManifest{};
        if (!texture_manifests.contains(face.texture_id)) {
            tex.image = device.create_image({
                .format = daxa::Format::R8G8B8A8_SRGB,
                .size = {texture.size.x, texture.size.y, 1},
                .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST,
                .name = "image",
            });
            texture_manifests[face.texture_id] = tex;
        } else {
            tex = texture_manifests[face.texture_id];
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
            v.model_id = model_manifests.size() - 1;

            v.pos = {v0.x, v0.y, v0.z};
            v.uv = {uv0.x, uv0.y};
            vertices.push_back(v);

            v.pos = {v1.x, v1.y, v1.z};
            v.uv = {uv1.x, uv1.y};
            vertices.push_back(v);

            v.pos = {v2.x, v2.y, v2.z};
            v.uv = {uv2.x, uv2.y};
            vertices.push_back(v);

            v1 = v2;
            uv1 = uv2;
        }
    }
    model_manifests.back().finalize(device);

    auto model_manifests_buffer_id = device.create_buffer({
        .size = sizeof(GpuModelManifest) * model_manifests.size(),
        .name = "model manifests",
    });
    auto task_model_manifests = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&model_manifests_buffer_id, 1}},
        .name = "task model manifests",
    });

    auto task_model_voxels = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&model_manifests.back().voxel_buffer, 1}},
        .name = "task model voxels",
    });

    auto cube_indices = std::vector<uint16_t>{0, 1, 2, 3, 4, 5, 6, 1};
    auto cube_indices_buffer_id = device.create_buffer({
        .size = sizeof(uint16_t) * cube_indices.size(),
        .name = "cube_indices",
    });
    auto task_cube_indices = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&cube_indices_buffer_id, 1}},
        .name = "task cube indices",
    });

    auto triangle_buffer_id = device.create_buffer({
        .size = vertices.size() * sizeof(MyVertex),
        .name = "my vertex data",
    });
    auto task_vertex_buffer = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&triangle_buffer_id, 1}},
        .name = "task vertex buffer",
    });

    auto frame_constants = FrameConstants{};
    frame_constants.sampler_llr = sampler_llr;

    auto frame_constants_buffer_id = device.create_buffer({
        .size = sizeof(FrameConstants),
        .name = "frame constants",
    });
    auto task_frame_constants = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&frame_constants_buffer_id, 1}},
        .name = "task frame constants",
    });

    auto task_textures = daxa::TaskImage{{.name = "textures"}};
    std::vector<daxa::ImageId> texture_images;
    for (auto [bsp_id, tex] : texture_manifests)
        texture_images.push_back(tex.image);
    task_textures.set_images({.images = texture_images});

    auto loop_task_graph = daxa::TaskGraph({
        .device = device,
        .swapchain = swapchain,
        .name = "loop",
    });

    loop_task_graph.use_persistent_buffer(task_frame_constants);
    loop_task_graph.use_persistent_buffer(task_vertex_buffer);
    loop_task_graph.use_persistent_image(task_swapchain_image);
    loop_task_graph.use_persistent_image(task_depth_image);
    loop_task_graph.use_persistent_image(task_textures);
    loop_task_graph.use_persistent_buffer(task_model_manifests);
    loop_task_graph.use_persistent_buffer(task_model_voxels);
    loop_task_graph.use_persistent_buffer(task_cube_indices);
    upload_frame_constants_task(loop_task_graph, task_frame_constants, &frame_constants);

    auto task_processed_vertex_buffer = loop_task_graph.create_transient_buffer({
        .size = uint32_t(vertices.size() * sizeof(MyVertex)),
        .name = "processed vertices",
    });

    loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_frame_constants),
            daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_vertex_buffer),
            daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_READ, task_model_manifests),
            daxa::inl_attachment(daxa::TaskBufferAccess::COMPUTE_SHADER_WRITE, task_processed_vertex_buffer),
        },
        .task = [&](daxa::TaskInterface ti) {
            ti.recorder.set_pipeline(*voxelize_preprocess_pipe);
            ti.recorder.push_constant(VoxelizeDrawPush{
                .frame_constants = ti.device_address(task_frame_constants.view()).value(),
                .model_manifests = ti.device_address(task_model_manifests.view()).value(),
                .vertices = ti.device_address(task_vertex_buffer.view()).value(),
                .processed_vertices = ti.device_address(task_processed_vertex_buffer).value(),
                .triangle_count = uint32_t(vertices.size() / 3),
            });
            ti.recorder.dispatch({uint32_t(vertices.size() / 3 + 127) / 128, 1, 1});
        },
        .name = "preprocess verts",
    });

    loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, task_model_voxels),
        },
        .task = [&](daxa::TaskInterface ti) {
            for (auto const& model : model_manifests) {
                auto buffer_size = ti.device.info(model.voxel_buffer).value().size;
                ti.recorder.clear_buffer({.buffer = model.voxel_buffer, .size = buffer_size});
            }
        },
        .name = "clear voxel volumes",
    });

    loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ, task_frame_constants),
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ, task_model_manifests),
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ, task_processed_vertex_buffer),
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ_WRITE, task_model_voxels),
            daxa::inl_attachment(daxa::TaskImageAccess::FRAGMENT_SHADER_SAMPLED, task_textures),
        },
        .task = [&](daxa::TaskInterface ti) {
            daxa::RenderCommandRecorder render_recorder = std::move(ti.recorder).begin_renderpass({
                .render_area = {.width = 256, .height = 256},
            });
            render_recorder.set_pipeline(*voxelize_draw_pipe);
            render_recorder.push_constant(VoxelizeDrawPush{
                .frame_constants = ti.device_address(task_frame_constants.view()).value(),
                .model_manifests = ti.device_address(task_model_manifests.view()).value(),
                .processed_vertices = ti.device_address(task_processed_vertex_buffer).value(),
                .triangle_count = uint32_t(vertices.size() / 3),
            });
            render_recorder.draw({.vertex_count = uint32_t(vertices.size())});
            ti.recorder = std::move(render_recorder).end_renderpass();
        },
        .name = "voxelize",
    });

    loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, task_swapchain_image),
            daxa::inl_attachment(daxa::TaskImageAccess::DEPTH_ATTACHMENT, task_depth_image),
            daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, task_frame_constants),

            daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, task_vertex_buffer),
            daxa::inl_attachment(daxa::TaskImageAccess::FRAGMENT_SHADER_SAMPLED, task_textures),

            daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, task_model_manifests),
            daxa::inl_attachment(daxa::TaskBufferAccess::VERTEX_SHADER_READ, task_model_voxels),
            daxa::inl_attachment(daxa::TaskBufferAccess::GRAPHICS_SHADER_READ, task_cube_indices),
        },
        .task = [&](daxa::TaskInterface ti) {
            auto const& image_attach_info = ti.get(task_swapchain_image);
            auto const& depth_attach_info = ti.get(task_depth_image);
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

            if (app.settings.show_mesh) {
                render_recorder.set_pipeline(*triangle_draw_pipe);
                render_recorder.push_constant(TriDrawPush{
                    .frame_constants = ti.device_address(task_frame_constants.view()).value(),
                    .vertices = ti.device_address(task_vertex_buffer.view()).value(),
                    .overlay = {0, 0, 0, 0},
                });
                render_recorder.draw({.vertex_count = uint32_t(vertices.size())});
            }

            if (app.settings.show_wireframe) {
                render_recorder.set_pipeline(*triangle_wire_draw_pipe);
                render_recorder.push_constant(TriDrawPush{
                    .frame_constants = ti.device_address(task_frame_constants.view()).value(),
                    .vertices = ti.device_address(task_vertex_buffer.view()).value(),
                    .overlay = {1, 1, 1, 0.1f},
                });
                render_recorder.draw({.vertex_count = uint32_t(vertices.size())});
            }

            if (app.settings.show_voxels) {
                render_recorder.set_pipeline(*box_draw_pipe);
                render_recorder.push_constant(BoxDrawPush{
                    .frame_constants = ti.device_address(task_frame_constants.view()).value(),
                    .model_manifests = ti.device_address(task_model_manifests.view()).value(),
                });
                render_recorder.set_index_buffer({
                    .id = ti.get(task_cube_indices).ids[0],
                    .index_type = daxa::IndexType::uint16,
                });
                render_recorder.draw_indexed({.index_count = 8, .instance_count = uint32_t(model_manifests.size())});
            }

            ti.recorder = std::move(render_recorder).end_renderpass();
        },
        .name = "draw",
    });

    loop_task_graph.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, task_swapchain_image),
        },
        .task = [&](daxa::TaskInterface const& ti) {
            imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, ti.get(task_swapchain_image).ids[0], app.window.size_x, app.window.size_y);
        },
        .name = "ImGui Task",
    });

    loop_task_graph.submit({});
    loop_task_graph.present({});
    loop_task_graph.complete({});

    {
        auto upload_task_graph = daxa::TaskGraph({
            .device = device,
            .name = "upload",
        });
        upload_task_graph.use_persistent_buffer(task_vertex_buffer);
        upload_task_graph.use_persistent_image(task_textures);
        upload_task_graph.use_persistent_buffer(task_model_manifests);
        upload_task_graph.use_persistent_buffer(task_cube_indices);
        upload_vector_task(upload_task_graph, task_vertex_buffer, &vertices);
        upload_vector_task(upload_task_graph, task_cube_indices, &cube_indices);

        std::vector<GpuModelManifest> gpu_model_manifests;
        gpu_model_manifests.reserve(model_manifests.size());
        for (auto const& model : model_manifests) {
            gpu_model_manifests.push_back({
                .voxels = device.get_device_address(model.voxel_buffer).value(),
                .aabb_min = std::bit_cast<daxa_f32vec3>(model.aabb_min),
                .aabb_max = std::bit_cast<daxa_f32vec3>(model.aabb_max),
            });
        }
        upload_vector_task(upload_task_graph, task_model_manifests, &gpu_model_manifests);

        upload_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, task_textures),
            },
            .task = [=](daxa::TaskInterface ti) {
                for (auto [bsp_id, tex] : texture_manifests) {
                    auto bsp_tex = voxlife::bsp::get_texture_data(bsp_handle, bsp_id);
                    auto size = size_t(bsp_tex.size.x * bsp_tex.size.y * sizeof(uint32_t));
                    auto staging_buffer_id = ti.device.create_buffer({
                        .size = size,
                        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                        .name = "my staging buffer",
                    });
                    ti.recorder.destroy_buffer_deferred(staging_buffer_id);
                    auto* buffer_ptr = ti.device.buffer_host_address_as<unsigned char>(staging_buffer_id).value();
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

        upload_task_graph.submit({});
        upload_task_graph.complete({});
        upload_task_graph.execute({});
    }

    auto t0 = Clock::now();

    while (true) {
        if (!app.update())
            break;

        if (app.window.swapchain_out_of_date) {
            recreate_render_image(depth_image, task_depth_image);
            swapchain.resize();
            app.window.swapchain_out_of_date = false;
        }
        auto swapchain_image = swapchain.acquire_next_image();
        if (swapchain_image.is_empty())
            continue;

        pipeline_manager.reload_all();

        auto reload_result = pipeline_manager.reload_all();
        if (auto* reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
            std::cout << reload_err->message << std::endl;
        } else if (auto* reload_success = daxa::get_if<daxa::PipelineReloadSuccess>(&reload_result)) {
            std::cout << "SUCCESS!" << std::endl;
        }

        frame_constants.world_to_view = std::bit_cast<daxa_f32mat4x4>(app.player.camera.vrot_mat * app.player.camera.vtrn_mat);
        frame_constants.view_to_world = std::bit_cast<daxa_f32mat4x4>(glm::inverse(app.player.camera.vrot_mat * app.player.camera.vtrn_mat));
        frame_constants.view_to_clip = std::bit_cast<daxa_f32mat4x4>(app.player.camera.proj_mat);
        frame_constants.clip_to_view = std::bit_cast<daxa_f32mat4x4>(glm::inverse(app.player.camera.proj_mat));
        auto t1 = Clock::now();
        frame_constants.time = std::chrono::duration<float>(t1 - t0).count();

        task_swapchain_image.set_images({.images = std::span{&swapchain_image, 1}});
        loop_task_graph.execute({});
        device.collect_garbage();

        // using namespace std::chrono_literals;
        // std::this_thread::sleep_for(50ms);
    }

    device.wait_idle();
    device.collect_garbage();

    for (auto const& [bsp_id, tex] : texture_manifests)
        device.destroy_image(tex.image);
    device.destroy_sampler(sampler_llr);
    for (auto const& model : model_manifests)
        device.destroy_buffer(model.voxel_buffer);
    device.destroy_buffer(model_manifests_buffer_id);
    device.destroy_buffer(cube_indices_buffer_id);
    device.destroy_buffer(triangle_buffer_id);
    device.destroy_buffer(frame_constants_buffer_id);
    device.destroy_image(depth_image);

    glfwDestroyWindow(app.window.glfw_ptr);
    glfwTerminate();
}
