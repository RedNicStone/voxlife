#pragma once

#include "player.hpp"
#include <chrono>
#include <daxa/daxa.hpp>

struct WindowInfo {
    struct GLFWwindow* glfw_ptr;
    uint32_t size_x{}, size_y{};
    bool swapchain_out_of_date = false;
};

using Clock = std::chrono::high_resolution_clock;

struct Application {
    Clock::time_point t0;
    WindowInfo window = {.size_x = 800, .size_y = 600};
    bool paused = true;
    Player3D player = {
        .pos = {0.0f, 0.0f, 0.0f},
        .rot = {0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f},
    };

    struct Settings {
        bool show_mesh = true;
        bool show_wireframe = true;
        bool show_voxels = true;
        bool use_msaa = true;
        bool use_nearest = false;
    };
    Settings settings{};

    Application();
    ~Application();
    void toggle_pause();
    bool update();
};

auto get_native_handle(GLFWwindow* glfw_window_ptr) -> daxa::NativeWindowHandle;
auto get_native_platform(GLFWwindow* glfw_window_ptr) -> daxa::NativeWindowPlatform;
