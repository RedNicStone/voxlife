#include "test.h"

#include <chrono>
#include <daxa/daxa.hpp>

// Include the pipeline manager
#include <daxa/pipeline.hpp>
#include <daxa/types.hpp>
#include <daxa/utils/pipeline_manager.hpp>
// We'll also include iostream, since we now use it
#include <iostream>

// We're going to use another optional feature of Daxa,
// called TaskGraph. We'll explain more below.
#include <daxa/utils/task_graph.hpp>

#include "shaders/shared.inl"
#include "voxel/test/player.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

auto get_native_handle(GLFWwindow* glfw_window_ptr) -> daxa::NativeWindowHandle {
#if defined(_WIN32)
    return glfwGetWin32Window(glfw_window_ptr);
#elif defined(__linux__)
    return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetX11Window(glfw_window_ptr));
#endif
}

auto get_native_platform(GLFWwindow* /*unused*/) -> daxa::NativeWindowPlatform {
#if defined(_WIN32)
    return daxa::NativeWindowPlatform::WIN32_API;
#elif defined(__linux__)
    return daxa::NativeWindowPlatform::XLIB_API;
#endif
}

#include <imgui.h>
#include <imgui_impl_glfw.h>

struct WindowInfo {
    GLFWwindow* glfw_ptr;
    daxa::u32 size_x{}, size_y{};
    bool swapchain_out_of_date = false;
};

void upload_vertces_data_task(daxa::TaskGraph& tg, daxa::TaskBufferView vertices, std::vector<MyVertex> const* data) {
    tg.add_task({
        .attachments = {
            daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, vertices),
        },
        .task = [=](daxa::TaskInterface ti) {
            auto size = sizeof(MyVertex) * data->size();
            auto staging_buffer_id = ti.device.create_buffer({
                .size = size,
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "my staging buffer",
            });
            ti.recorder.destroy_buffer_deferred(staging_buffer_id);
            auto* buffer_ptr = ti.device.buffer_host_address_as<MyVertex>(staging_buffer_id).value();
            memcpy(buffer_ptr, data->data(), size);
            daxa::TaskBufferAttachmentInfo const& buffer_at_info = ti.get(vertices);
            std::span<daxa::BufferId const> runtime_ids = buffer_at_info.ids;
            daxa::BufferId id = runtime_ids[0];
            ti.recorder.copy_buffer_to_buffer({
                .src_buffer = staging_buffer_id,
                .dst_buffer = id,
                .size = size,
            });
        },
        .name = "upload vertices",
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

struct DrawToSwapchainTask : DrawToSwapchainH::Task {
    AttachmentViews views = {};
    daxa::RasterPipeline* pipeline = {};
    daxa::RasterPipeline* wire_pipeline = {};
    size_t vert_n = {};
    void callback(daxa::TaskInterface ti) {
        auto const& image_attach_info = ti.get(AT.color_target);
        auto const& depth_attach_info = ti.get(AT.depth_target);
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
        render_recorder.set_pipeline(*pipeline);
        render_recorder.push_constant(MyPushConstant{
            .attachments = ti.attachment_shader_blob,
            .overlay = {0, 0, 0, 0},
        });
        render_recorder.draw({.vertex_count = uint32_t(vert_n)});
        render_recorder.set_pipeline(*wire_pipeline);
        render_recorder.push_constant(MyPushConstant{
            .attachments = ti.attachment_shader_blob,
            .overlay = {1, 1, 1, 0.1},
        });
        render_recorder.draw({.vertex_count = uint32_t(vert_n)});
        ti.recorder = std::move(render_recorder).end_renderpass();
    }
};

struct Application {
    WindowInfo window = {.size_x = 800, .size_y = 600};
    bool paused = true;
    Player3D player = {
        .pos = {0.0f, 0.0f, 0.0f},
        .rot = {0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f},
    };

    void toggle_pause() {
        paused = !paused;
        glfwSetCursorPos(window.glfw_ptr, static_cast<double>(window.size_x / 2), static_cast<double>(window.size_y / 2));
        glfwSetInputMode(window.glfw_ptr, GLFW_CURSOR, !paused ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        glfwSetInputMode(window.glfw_ptr, GLFW_RAW_MOUSE_MOTION, !paused);
    }
};


void test_voxelization_gui(voxlife::bsp::bsp_handle bsp_handle) {
    auto app = Application{};
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    app.window.glfw_ptr = glfwCreateWindow(
        static_cast<int>(app.window.size_x),
        static_cast<int>(app.window.size_y),
        "voxlife", nullptr, nullptr);
    glfwSetWindowUserPointer(app.window.glfw_ptr, &app);
    glfwSetWindowSizeCallback(
        app.window.glfw_ptr,
        [](GLFWwindow* glfw_window, int width, int height) {
            auto& app = *reinterpret_cast<Application*>(glfwGetWindowUserPointer(glfw_window));
            app.window.swapchain_out_of_date = true;
            app.window.size_x = static_cast<daxa::u32>(width);
            app.window.size_y = static_cast<daxa::u32>(height);
        });
    auto* native_window_handle = get_native_handle(app.window.glfw_ptr);
    auto native_window_platform = get_native_platform(app.window.glfw_ptr);

    daxa::Instance instance = daxa::create_instance({});
    daxa::Device device = instance.create_device_2(instance.choose_device({}, {}));

    daxa::Swapchain swapchain = device.create_swapchain({
        .native_window = native_window_handle,
        .native_window_platform = native_window_platform,
        .present_mode = daxa::PresentMode::FIFO,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .name = "my swapchain",
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
        .name = "my pipeline manager",
    });
    std::shared_ptr<daxa::RasterPipeline> triangle_draw_pipe;
    std::shared_ptr<daxa::RasterPipeline> triangle_wire_draw_pipe;
    {
        auto result = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .color_attachments = {{.format = swapchain.get_format()}},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D24_UNORM_S8_UINT,
                .enable_depth_write = true,
            },
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST,
                .polygon_mode = daxa::PolygonMode::FILL,
            },
            .push_constant_size = sizeof(MyPushConstant),
            .name = "my pipeline",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            return;
        }
        triangle_draw_pipe = result.value();

        result = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .fragment_shader_info = daxa::ShaderCompileInfo{.source = daxa::ShaderFile{"main.glsl"}},
            .color_attachments = {{.format = swapchain.get_format()}},
            .depth_test = daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D24_UNORM_S8_UINT,
                .enable_depth_write = true,
            },
            .raster = {
                .primitive_topology = daxa::PrimitiveTopology::TRIANGLE_LIST, .polygon_mode = daxa::PolygonMode::LINE,
                // .depth_bias_enable = true,
                // .depth_bias_slope_factor = 0.5f,
            },
            .push_constant_size = sizeof(MyPushConstant),
            .name = "my pipeline",
        });
        if (result.is_err()) {
            std::cerr << result.message() << std::endl;
            return;
        }
        triangle_wire_draw_pipe = result.value();
    }

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
        .format = daxa::Format::D24_UNORM_S8_UINT,
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

    struct ModelManifest {
        daxa::BufferId buffer;

        glm::vec3 aabb_min{};
        glm::vec3 aabb_max{};

        void finalize(daxa::Device& device) {
            glm::uvec3 volume_extent = glm::round(aabb_max - aabb_min);
            buffer = device.create_buffer({
                .size = volume_extent.x * volume_extent.y * volume_extent.z * sizeof(Voxel),
                .name = "model buffer",
            });
        }
    };
    std::vector<ModelManifest> model_manifests;

    for (auto& face : faces) {
        auto texture_name = voxlife::bsp::get_texture_name(bsp_handle, face.texture_id);
        auto texture = voxlife::bsp::get_texture_data(bsp_handle, face.texture_id);

        if (texture_name == "SKY" || texture_name == "sky")
            continue;

        glm::vec3 face_aabb_min = glm::floor(face.vertices[0] * (0.0254f / 0.1f));
        glm::vec3 face_aabb_max = glm::ceil(face.vertices[0] * (0.0254f / 0.1f));
        for (auto const& v : face.vertices) {
            face_aabb_min = glm::min(face_aabb_min, glm::floor(v * (0.0254f / 0.1f)));
            face_aabb_max = glm::max(face_aabb_max, glm::ceil(v * (0.0254f / 0.1f)));
        }

        if (model_manifests.empty()) {
            ModelManifest model;
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
        v0 = face.vertices[0] * 0.0254f;
        v1 = face.vertices[1] * 0.0254f;

        glm::vec2 uv0, uv1, uv2;
        uv0.x = (glm::dot(face.texture_coords.x.axis, face.vertices[0]) + face.texture_coords.x.shift) / float(texture.size.x);
        uv0.y = (glm::dot(face.texture_coords.y.axis, face.vertices[0]) + face.texture_coords.y.shift) / float(texture.size.y);
        uv1.x = (glm::dot(face.texture_coords.x.axis, face.vertices[1]) + face.texture_coords.x.shift) / float(texture.size.x);
        uv1.y = (glm::dot(face.texture_coords.y.axis, face.vertices[1]) + face.texture_coords.y.shift) / float(texture.size.y);

        for (int i = 0; i < triangle_count; ++i) {
            v2 = face.vertices[i + 2] * 0.0254f;
            uv2.x = (glm::dot(face.texture_coords.x.axis, face.vertices[i + 2]) + face.texture_coords.x.shift) / float(texture.size.x);
            uv2.y = (glm::dot(face.texture_coords.y.axis, face.vertices[i + 2]) + face.texture_coords.y.shift) / float(texture.size.y);
            auto v = MyVertex{};
            v.tex_id = daxa_ImageViewIndex(tex.image.default_view().index);
            v.model_id = model_manifests.size() - 1;

            v.position = {-v0.x, v0.y, v0.z};
            v.uv = {uv0.x, uv0.y};
            vertices.push_back(v);

            v.position = {-v1.x, v1.y, v1.z};
            v.uv = {uv1.x, uv1.y};
            vertices.push_back(v);

            v.position = {-v2.x, v2.y, v2.z};
            v.uv = {uv2.x, uv2.y};
            vertices.push_back(v);

            v1 = v2;
            uv1 = uv2;
        }
    }
    model_manifests.back().finalize(device);

    auto task_models = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&model_manifests.back().buffer, 1}},
        .name = "task models",
    });

    auto triangle_buffer_id = device.create_buffer({
        .size = vertices.size() * sizeof(MyVertex),
        .name = "my vertex data",
    });
    auto task_vertex_buffer = daxa::TaskBuffer({
        .initial_buffers = {.buffers = std::span{&triangle_buffer_id, 1}},
        .name = "task vertex buffer",
    });

    glfwSetCursorPosCallback(
        app.window.glfw_ptr,
        [](GLFWwindow* glfw_window, double x, double y) {
            auto& app = *reinterpret_cast<Application*>(glfwGetWindowUserPointer(glfw_window));

            if (!app.paused) {
                float center_x = static_cast<float>(app.window.size_x / 2);
                float center_y = static_cast<float>(app.window.size_y / 2);
                auto offset = glm::vec2{float(x) - center_x, center_y - float(y)};
                app.player.on_mouse_move(offset.x, offset.y);
                glfwSetCursorPos(glfw_window, center_x, center_y);
            }
        });
    glfwSetMouseButtonCallback(
        app.window.glfw_ptr,
        [](GLFWwindow* glfw_window, int key_id, int action, int) {
            auto& app = *reinterpret_cast<Application*>(glfwGetWindowUserPointer(glfw_window));
            auto& io = ImGui::GetIO();
            if (io.WantCaptureKeyboard)
                return;
            // if (app.paused && key_id == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS)
            //     app.toggle_pause();
        });
    glfwSetKeyCallback(
        app.window.glfw_ptr,
        [](GLFWwindow* glfw_window, int key_id, int, int action, int) {
            auto& app = *reinterpret_cast<Application*>(glfwGetWindowUserPointer(glfw_window));
            auto& io = ImGui::GetIO();
            if (io.WantCaptureKeyboard)
                return;
            if (key_id == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
                app.toggle_pause();
            if (!app.paused) {
                app.player.on_key(key_id, action);
            }
        });
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(app.window.glfw_ptr, true);

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
    upload_frame_constants_task(loop_task_graph, task_frame_constants, &frame_constants);
    loop_task_graph.add_task(DrawToSwapchainTask{
        .views = std::array{
            daxa::attachment_view(DrawToSwapchainH::AT.frame_constants, task_frame_constants),
            daxa::attachment_view(DrawToSwapchainH::AT.vertices, task_vertex_buffer),
            daxa::attachment_view(DrawToSwapchainH::AT.color_target, task_swapchain_image),
            daxa::attachment_view(DrawToSwapchainH::AT.depth_target, task_depth_image),
            daxa::attachment_view(DrawToSwapchainH::AT.textures, task_textures),
        },
        .pipeline = triangle_draw_pipe.get(),
        .wire_pipeline = triangle_wire_draw_pipe.get(),
        .vert_n = vertices.size(),
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
        upload_vertces_data_task(upload_task_graph, task_vertex_buffer, &vertices);

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

    using Clock = std::chrono::high_resolution_clock;
    auto t0 = Clock::now();

    while (true) {
        glfwPollEvents();
        if (glfwWindowShouldClose(app.window.glfw_ptr) != 0) {
            break;
        }
        auto t1 = Clock::now();
        float dt = std::chrono::duration<float>(t1 - t0).count();
        t0 = t1;

        if (app.window.swapchain_out_of_date) {
            recreate_render_image(depth_image, task_depth_image);
            swapchain.resize();
            app.window.swapchain_out_of_date = false;
        }
        auto swapchain_image = swapchain.acquire_next_image();
        if (swapchain_image.is_empty()) {
            continue;
        }

        pipeline_manager.reload_all();

        auto reload_result = pipeline_manager.reload_all();
        if (auto* reload_err = daxa::get_if<daxa::PipelineReloadError>(&reload_result)) {
            std::cout << reload_err->message << std::endl;
        } else if (auto* reload_success = daxa::get_if<daxa::PipelineReloadSuccess>(&reload_result)) {
            std::cout << "SUCCESS!" << std::endl;
        }

        app.player.update(dt);
        app.player.camera.set_pos(app.player.pos);
        app.player.camera.set_rot(app.player.rot.x, app.player.rot.y);
        app.player.camera.resize(app.window.size_x, app.window.size_y);

        frame_constants.world_to_view = std::bit_cast<daxa_f32mat4x4>(app.player.camera.vrot_mat * app.player.camera.vtrn_mat);
        frame_constants.view_to_clip = std::bit_cast<daxa_f32mat4x4>(app.player.camera.proj_mat);

        task_swapchain_image.set_images({.images = std::span{&swapchain_image, 1}});
        loop_task_graph.execute({});
        device.collect_garbage();
    }

    device.wait_idle();
    device.collect_garbage();
    device.destroy_buffer(triangle_buffer_id);
    device.destroy_buffer(frame_constants_buffer_id);
    device.destroy_image(depth_image);

    glfwDestroyWindow(app.window.glfw_ptr);
    glfwTerminate();
}
