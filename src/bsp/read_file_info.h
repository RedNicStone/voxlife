
#ifndef VOXLIFE_READ_FILE_INFO_H
#define VOXLIFE_READ_FILE_INFO_H

#include <bsp/primitives.h>
#include <wad/read_file.h>

#include <glm/vec2.hpp>

#include <string_view>
#include <span>
#include <vector>


namespace voxlife::bsp {

    struct bsp_info {
#if defined(_WIN32)
        void* hFile;
        void* hMap;
#else
        int bsp_file = -1;
#endif
        size_t file_size = 0;

        union {
            const uint8_t *file_data = nullptr;
            const header *header;
        };

        const void* lump_begins[lump_type::LUMP_MAX];
        const void* lump_ends[lump_type::LUMP_MAX];

        std::string_view                   entities_str;
        std::span<const lump_plane>        planes;
        std::span<const lump_mip_texture>  textures;
        std::span<const lump_vertex>       vertices;
        std::span<const lump_node>         nodes;
        std::span<const lump_texture_info> texture_infos;
        std::span<const lump_face>         faces;
        std::span<const lump_clip_node>    clip_nodes;
        std::span<const lump_leaf>         leafs;
        std::span<const lump_mark_surface> mark_surfaces;
        std::span<const lump_edge>         edges;
        std::span<const lump_surf_edge>    surface_edges;
        std::span<const lump_model>        models;

        struct loaded_texture {
            std::vector<glm::u8vec3> data;

            glm::u32vec2 size;
        };

        std::vector<loaded_texture> loaded_textures;
    };

}


#endif //VOXLIFE_READ_FILE_INFO_H
