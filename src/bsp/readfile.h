
#ifndef VOXLIFE_BSP_READFILE_H
#define VOXLIFE_BSP_READFILE_H

#include <wad/readfile.h>

#include <string_view>
#include <vector>
#include <span>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>


namespace voxlife::bsp {

    typedef struct bsp_handle_T *bsp_handle;

    void open_file(std::string_view filename, wad::wad_handle resources, bsp_handle* handle);

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

    std::vector<face> get_model_faces(bsp_handle handle, uint32_t model_id);
    texture get_texture_data(bsp_handle handle, uint32_t texture_id);
}

#endif //VOXLIFE_BSP_READFILE_H
