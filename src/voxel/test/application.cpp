#include "application.h"

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

Application::Application() {
    t0 = Clock::now();
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window.glfw_ptr = glfwCreateWindow(
        static_cast<int>(window.size_x),
        static_cast<int>(window.size_y),
        "voxlife", nullptr, nullptr);
    glfwSetWindowUserPointer(window.glfw_ptr, this);
    glfwSetWindowSizeCallback(
        window.glfw_ptr,
        [](GLFWwindow* glfw_window, int width, int height) {
            auto* self = reinterpret_cast<Application*>(glfwGetWindowUserPointer(glfw_window));
            self->window.swapchain_out_of_date = true;
            self->window.size_x = static_cast<daxa::u32>(width);
            self->window.size_y = static_cast<daxa::u32>(height);
        });
    glfwSetCursorPosCallback(
        window.glfw_ptr,
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
        window.glfw_ptr,
        [](GLFWwindow* glfw_window, int key_id, int action, int) {
            auto& app = *reinterpret_cast<Application*>(glfwGetWindowUserPointer(glfw_window));
            auto& io = ImGui::GetIO();
            if (io.WantCaptureKeyboard)
                return;
            // if (app.paused && key_id == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS)
            //     app.toggle_pause();
        });
    glfwSetKeyCallback(
        window.glfw_ptr,
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

    auto ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGui_ImplGlfw_InitForVulkan(window.glfw_ptr, true);
}

Application::~Application() {
    ImGui_ImplGlfw_Shutdown();
    glfwDestroyWindow(window.glfw_ptr);
    glfwTerminate();
}

void Application::toggle_pause() {
    paused = !paused;
    glfwSetCursorPos(window.glfw_ptr, static_cast<double>(window.size_x / 2), static_cast<double>(window.size_y / 2));
    glfwSetInputMode(window.glfw_ptr, GLFW_CURSOR, !paused ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwSetInputMode(window.glfw_ptr, GLFW_RAW_MOUSE_MOTION, !paused);
}

bool Application::update() {
    glfwPollEvents();

    auto t1 = Clock::now();
    float dt = std::chrono::duration<float>(t1 - t0).count();
    t0 = t1;

    player.update(dt);
    player.camera.set_pos(player.pos);
    player.camera.set_rot(player.rot.x, player.rot.y);
    player.camera.resize(window.size_x, window.size_y);

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("menu");
    ImGui::Checkbox("show mesh", &settings.show_mesh);
    ImGui::Checkbox("show wireframe", &settings.show_wireframe);
    ImGui::Checkbox("show voxels", &settings.show_voxels);
    ImGui::End();

    ImGui::Render();

    return !glfwWindowShouldClose(window.glfw_ptr);
}
