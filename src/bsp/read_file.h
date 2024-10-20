
#ifndef VOXLIFE_BSP_READ_FILE_H
#define VOXLIFE_BSP_READ_FILE_H

#include <wad/read_file.h>

#include <string_view>
#include <vector>
#include <span>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>


namespace voxlife::bsp {

    typedef struct bsp_handle_T *bsp_handle;

    void open_file(std::string_view filename, bsp_handle* handle);
    void release(bsp_handle handle);
    void load_textures(bsp_handle handle, std::span<wad::wad_handle> resources);

    struct face {
        enum type : int32_t {
            PLANE_X     = 0,
            PLANE_Y     = 1,
            PLANE_Z     = 2,
            PLANE_ANYX  = 3,
            PLANE_ANYY  = 4,
            PLANE_ANYZ  = 5
        } facing;

        struct texture_position {
            glm::vec3 axis;
            float shift;
        };

        glm::vec<2, texture_position> texture_coords;
        uint32_t texture_id;

        glm::vec3 normal;
        std::vector<glm::vec3> vertices;
    };

    struct texture {
        std::span<glm::u8vec3> data;

        glm::u32vec2 size;
    };

    struct entity {
        struct key_value_pair {
            std::string_view key;
            std::string_view value;
        };

        std::vector<key_value_pair> pairs;
    };

    struct aabb {
        glm::vec3 min;
        glm::vec3 max;
    };

    std::vector<face> get_model_faces(bsp_handle handle, uint32_t model_id);
    aabb get_model_aabb(bsp_handle handle, uint32_t model_id);
    texture get_texture_data(bsp_handle handle, uint32_t texture_id);
    texture get_texture_data(bsp_handle handle, std::string_view texture_id);
    std::string_view get_texture_name(bsp_handle handle, uint32_t texture_id);
    uint32_t get_texture_id(bsp_handle handle, std::string_view name);
    std::vector<entity> get_entities(bsp_handle handle);
}

#endif //VOXLIFE_BSP_READ_FILE_H
