#pragma once

#define GLM_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <numbers>

struct Camera3D {
    float fov = 98.6f, aspect = 1.0f;
    float near_clip = 0.01f, far_clip = 1000.0f;
    glm::mat4 proj_mat{};
    glm::mat4 vtrn_mat{};
    glm::mat4 vrot_mat{};

    void resize(int size_x, int size_y) {
        aspect = static_cast<float>(size_x) / static_cast<float>(size_y);
        proj_mat = glm::perspective(glm::radians(fov), aspect, near_clip, far_clip);
    }
    void set_pos(glm::vec3 pos) {
        vtrn_mat = glm::translate(glm::mat4(1), glm::vec3(pos.x, pos.y, pos.z));
    }
    void set_rot(float x, float y) {
        vrot_mat = glm::rotate(glm::rotate(glm::mat4(1), y, {1, 0, 0}), x, {0, 0, 1});
    }
    glm::mat4 get_vp() {
        return proj_mat * vrot_mat * vtrn_mat;
    }
};

namespace input {
    struct Keybinds {
        int move_frwd, move_back;
        int move_left, move_rigt;
        int move_upwd, move_down;
        int toggle_pause;
        int toggle_sprint;
    };

    static inline constexpr Keybinds DEFAULT_KEYBINDS{
        .move_frwd = GLFW_KEY_W,
        .move_back = GLFW_KEY_S,
        .move_left = GLFW_KEY_A,
        .move_rigt = GLFW_KEY_D,
        .move_upwd = GLFW_KEY_SPACE,
        .move_down = GLFW_KEY_LEFT_CONTROL,
        .toggle_pause = GLFW_KEY_ESCAPE,
        .toggle_sprint = GLFW_KEY_LEFT_SHIFT,
    };
} // namespace input

struct Player3D {
    Camera3D camera{};
    input::Keybinds keybinds = input::DEFAULT_KEYBINDS;
    glm::vec3 pos{0, 0, 0}, vel{}, rot{};
    float speed = 8.0f, mouse_sens = 0.1f;
    float sprint_speed = 5.0f;
    float sin_rot_x = 0, cos_rot_x = 1;

    struct MoveFlags {
        uint8_t frwd : 1, back : 1;
        uint8_t left : 1, rigt : 1;
        uint8_t upwd : 1, down : 1;
        uint8_t sprint : 1;
    } move{};

    void update(float dt) {
        auto delta_pos = speed * dt;
        if (move.sprint)
            delta_pos *= sprint_speed;
        if (move.frwd)
            pos.x += sin_rot_x * delta_pos, pos.y += cos_rot_x * delta_pos;
        if (move.back)
            pos.x -= sin_rot_x * delta_pos, pos.y -= cos_rot_x * delta_pos;
        if (move.left)
            pos.y -= sin_rot_x * delta_pos, pos.x += cos_rot_x * delta_pos;
        if (move.rigt)
            pos.y += sin_rot_x * delta_pos, pos.x -= cos_rot_x * delta_pos;
        if (move.upwd)
            pos.z -= delta_pos;
        if (move.down)
            pos.z += delta_pos;

        if (rot.y > std::numbers::pi_v<float>)
            rot.y = std::numbers::pi_v<float>;
        if (rot.y < 0)
            rot.y = 0;
    }
    void on_key(int key, int action) {
        if (key == keybinds.move_down)
            move.down = action != 0;
        if (key == keybinds.move_upwd)
            move.upwd = action != 0;
        if (key == keybinds.move_left)
            move.left = action != 0;
        if (key == keybinds.move_rigt)
            move.rigt = action != 0;
        if (key == keybinds.move_frwd)
            move.frwd = action != 0;
        if (key == keybinds.move_back)
            move.back = action != 0;
        if (key == keybinds.toggle_sprint)
            move.sprint = action != 0;
    }
    void on_mouse_move(float delta_x, float delta_y) {
        rot.x -= delta_x * mouse_sens * 0.0001f * camera.fov;
        rot.y += delta_y * mouse_sens * 0.0001f * camera.fov;
        sin_rot_x = std::sin(rot.x);
        cos_rot_x = std::cos(rot.x);
    }
};
