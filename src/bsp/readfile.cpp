
#include <bsp/readfile.h>
#include <bsp/primitives.h>

#include <stdexcept>
#include <format>
#include <iostream>
#include <fstream>
#include <span>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#endif


namespace voxlife::bsp {

    struct bsp_info {
#if defined(_WIN32)
        HANDLE hFile;
        HANDLE hMap;
#else
        int bsp_file = -1;
#endif
        size_t file_size = 0;

        union {
            const uint8_t *file_data = nullptr;
            const header *header;
        };

        wad::wad_handle resources;

        const void* lump_begins[lump_type::LUMP_MAX];
        const void* lump_ends[lump_type::LUMP_MAX];

        std::span<const entity>       entities;
        std::span<const plane>        planes;
        std::span<const mip_texture>  textures;
        std::span<const vertex>       vertices;
        std::span<const node>         nodes;
        std::span<const texture_info> texture_infos;
        std::span<const face>         faces;
        std::span<const clip_node>    clip_nodes;
        std::span<const leaf>         leafs;
        std::span<const mark_surface> mark_surfaces;
        std::span<const edge>         edges;
        std::span<const surf_edge>    surface_edges;
        std::span<const model>        models;
    };


    void parse_header(bsp_info &info) {
        if (info.header->version != header::bsp_version_halflife)
            throw std::runtime_error(std::format("Unsupported BSP version {}", info.header->version));

        for (int i = 0; i < lump_type::LUMP_MAX; ++i) {
            auto& lump = info.header->lumps[i];
            if (lump.offset < 0 || lump.length < 0)
                throw std::runtime_error(std::format("Lump {} is not valid", lump_names[i]));

            if (lump.offset + lump.length > info.file_size)
                throw std::runtime_error(std::format("Lump {} extends beyond end of file", lump_names[i]));

            info.lump_begins[i] = info.file_data + lump.offset;
            info.lump_ends[i]   = info.file_data + lump.offset + lump.length;
        }

        info.entities      = std::span(reinterpret_cast<const entity*>      (info.lump_begins[lump_type::LUMP_ENTITIES]),
                                       reinterpret_cast<const entity*>      (  info.lump_ends[lump_type::LUMP_ENTITIES]));
        info.planes        = std::span(reinterpret_cast<const plane*>       (info.lump_begins[lump_type::LUMP_PLANES]),
                                       reinterpret_cast<const plane*>       (  info.lump_ends[lump_type::LUMP_PLANES]));
        info.textures      = std::span(reinterpret_cast<const mip_texture*> (info.lump_begins[lump_type::LUMP_TEXTURES]),
                                       reinterpret_cast<const mip_texture*> (  info.lump_ends[lump_type::LUMP_TEXTURES]));
        info.vertices      = std::span(reinterpret_cast<const vertex*>      (info.lump_begins[lump_type::LUMP_VERTICES]),
                                       reinterpret_cast<const vertex*>      (  info.lump_ends[lump_type::LUMP_VERTICES]));
        info.nodes         = std::span(reinterpret_cast<const node*>        (info.lump_begins[lump_type::LUMP_NODES]),
                                       reinterpret_cast<const node*>        (  info.lump_ends[lump_type::LUMP_NODES]));
        info.texture_infos = std::span(reinterpret_cast<const texture_info*>(info.lump_begins[lump_type::LUMP_TEXINFO]),
                                       reinterpret_cast<const texture_info*>(  info.lump_ends[lump_type::LUMP_TEXINFO]));
        info.faces         = std::span(reinterpret_cast<const face*>        (info.lump_begins[lump_type::LUMP_FACES]),
                                       reinterpret_cast<const face*>        (  info.lump_ends[lump_type::LUMP_FACES]));
        info.clip_nodes    = std::span(reinterpret_cast<const clip_node*>   (info.lump_begins[lump_type::LUMP_CLIPNODES]),
                                       reinterpret_cast<const clip_node*>   (  info.lump_ends[lump_type::LUMP_CLIPNODES]));
        info.leafs         = std::span(reinterpret_cast<const leaf*>        (info.lump_begins[lump_type::LUMP_LEAFS]),
                                       reinterpret_cast<const leaf*>        (  info.lump_ends[lump_type::LUMP_LEAFS]));
        info.mark_surfaces = std::span(reinterpret_cast<const mark_surface*>(info.lump_begins[lump_type::LUMP_MARKSURFACES]),
                                       reinterpret_cast<const mark_surface*>(  info.lump_ends[lump_type::LUMP_MARKSURFACES]));
        info.edges         = std::span(reinterpret_cast<const edge*>        (info.lump_begins[lump_type::LUMP_EDGES]),
                                       reinterpret_cast<const edge*>        (  info.lump_ends[lump_type::LUMP_EDGES]));
        info.surface_edges = std::span(reinterpret_cast<const surf_edge*>   (info.lump_begins[lump_type::LUMP_SURFEDGES]),
                                       reinterpret_cast<const surf_edge*>   (  info.lump_ends[lump_type::LUMP_SURFEDGES]));
        info.models        = std::span(reinterpret_cast<const model*>       (info.lump_begins[lump_type::LUMP_MODELS]),
                                       reinterpret_cast<const model*>       (  info.lump_ends[lump_type::LUMP_MODELS]));
    }

    template<typename T>
    constexpr T& span_at(std::span<T> span, size_t index) {
        if (index >= span.size())
            throw std::out_of_range("Span index out of bounds");

        return span[index];
    }

    template<typename T>
    constexpr std::span<T> safe_subspan(std::span<T> span, size_t offset, size_t length) {
        if (offset + length > span.size())
            throw std::out_of_range("Span index out of bounds");

        return span.subspan(offset, length);
    }

    void read_map(bsp_info &info) {
        auto file = std::ofstream("map.ply", std::ios::out);

        file << "ply\n";
        file << "format ascii 1.0\n";
        file << "element vertex " << info.vertices.size() << "\n";
        file << "property float x\n";
        file << "property float y\n";
        file << "property float z\n";
        file << "element face " << info.faces.size() << "\n";
        file << "property list uint int vertex_index\n";
        file << "end_header\n";

        for (auto& vertex : info.vertices)
            file << vertex.x << " " << vertex.y << " " << vertex.z << "\n";

        for (auto& face : info.faces) {
            auto surface_edges = safe_subspan(info.surface_edges, face.first_edge, face.edge_count);

            file << face.edge_count;
            for (auto& surface_edge : surface_edges) {
                auto edge = span_at(info.edges, std::abs(surface_edge.edge));

                uint16_t edge_index = surface_edge.edge < 0 ? edge.vertex[0] : edge.vertex[1];
                file << " " << edge_index;
            }
            file << "\n";
        }
    }

    void read_textures(bsp_info &info) {
        auto* texture_lump_begin = reinterpret_cast<const uint8_t*>(info.lump_begins[lump_type::LUMP_TEXTURES]);
        auto* texture_lump_end = reinterpret_cast<const uint8_t*>(info.lump_ends[lump_type::LUMP_TEXTURES]);

        auto* texture_header = reinterpret_cast<const bsp::texture_header*>(texture_lump_begin);
        uint32_t texture_header_length = sizeof(bsp::texture_header) + (texture_header->mip_texture_count - 1) * sizeof(uint32_t);
        auto* texture_begin = texture_lump_begin + sizeof(bsp::texture_header);
        auto* texture_end = texture_lump_begin + texture_header_length;

        if (texture_end > texture_lump_end)
            throw std::runtime_error("Texture header extends beyond end of lump");

        for (; texture_begin < texture_end; texture_begin += sizeof(uint32_t)) {
            auto texture_offset = *reinterpret_cast<const uint32_t*>(texture_begin);
            auto* mip_texture = reinterpret_cast<const uint8_t*>(texture_lump_begin + texture_offset);
            auto* mip_texture_handle = reinterpret_cast<const bsp::mip_texture*>(mip_texture);

            if (reinterpret_cast<const uint8_t*>(mip_texture) > texture_lump_end)
                throw std::runtime_error("Mip texture extends beyond end of lump");

            const uint8_t* texture_data_begin;
            if (mip_texture_handle->offsets[0] & mip_texture_handle->offsets[1]
                & mip_texture_handle->offsets[2] & mip_texture_handle->offsets[3]) {
                // Texture is internal, do nothing
            } else {
                // Texture is external, load it
                mip_texture = reinterpret_cast<const uint8_t*>(wad::get_entry(info.resources, mip_texture_handle->name));
                if (mip_texture == nullptr) {
                    std::cout << std::format("Could not find texture '{}'\n", mip_texture_handle->name);
                    continue;
                }

                mip_texture_handle = reinterpret_cast<const bsp::mip_texture*>(mip_texture);
            }

            const uint32_t texel_count = mip_texture_handle->width * mip_texture_handle->height;
            auto color_data = mip_texture + mip_texture_handle->offsets[3] + texel_count / 64 + 2;

            if (color_data + 256 > texture_lump_end)
                throw std::runtime_error("Color data extends beyond end of lump");

            for (int i = 0; i < bsp::mip_texture::mip_levels; ++i) {
                const uint32_t texture_width  = mip_texture_handle->width >> (i * 2);
                const uint32_t texture_height = mip_texture_handle->height >> (i * 2);
                const uint32_t texture_length = texel_count >> (i * 2);
                const uint8_t* texture_data = mip_texture + mip_texture_handle->offsets[i];

                if (texture_data + texture_length > color_data)
                    throw std::runtime_error("Texture data extends beyond color data");

                auto file = std::ofstream(std::format("texture_{}_{}.ppm", mip_texture_handle->name, i), std::ios::out);
                file << "P3\n";
                file << texture_width << " " << texture_height << "\n255";
                for (uint32_t y = 0; y < texture_height; ++y) {
                    for (uint32_t x = 0; x < texture_width; ++x) {
                        uint8_t index = texture_data[y * texture_width + x];
                        uint8_t r = color_data[index * 3 + 0];
                        uint8_t g = color_data[index * 3 + 1];
                        uint8_t b = color_data[index * 3 + 2];
                        file << "\n" << +r << " " << +g << " " << +b;
                    }
                }
            }

            std::cout << mip_texture_handle->name << std::endl;
            std::cout << mip_texture_handle->width << std::endl;
            std::cout << mip_texture_handle->height << std::endl;
        }
    }

    void open_file(std::string_view filename, wad::wad_handle resources, bsp_handle* handle) {
        *handle = reinterpret_cast<bsp_handle>(new bsp_info{});
        auto& info = reinterpret_cast<bsp_info&>(**handle);


#if defined(_WIN32)

        LPVOID lpBasePtr;
        LARGE_INTEGER liFileSize;

        info.hFile = CreateFile(filename.data(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

        if (info.hFile == INVALID_HANDLE_VALUE) {
            auto err = GetLastError();
            throw std::runtime_error(std::format("CreateFile failed with error '{}'", err));
        }

        if (!GetFileSizeEx(info.hFile, &liFileSize)) {
            auto err = GetLastError();
            CloseHandle(info.hFile);
            throw std::runtime_error(std::format("GetFileSize failed with error '{}'", err));
        }

        if (liFileSize.QuadPart == 0) {
            CloseHandle(info.hFile);
            throw std::runtime_error(std::format("File is empty '{}'", filename));
        }

        info.hMap = CreateFileMapping(info.hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

        if (info.hMap == 0) {
            auto err = GetLastError();
            CloseHandle(info.hFile);
            throw std::runtime_error(std::format("CreateFileMapping failed with error '{}'", err));
        }

        lpBasePtr = MapViewOfFile(info.hMap, FILE_MAP_READ, 0, 0, 0);

        if (lpBasePtr == nullptr) {
            auto err = GetLastError();
            CloseHandle(info.hMap);
            CloseHandle(info.hFile);
            throw std::runtime_error(std::format("MapViewOfFile failed with error '{}'", err));
        }

        info.file_size = liFileSize.QuadPart;
        info.file_data = reinterpret_cast<uint8_t*>(lpBasePtr);

#else

        info.bsp_file = open(filename.data(), O_RDONLY);
        if (info.bsp_file < 0)
            throw std::runtime_error(std::format("Could not open file '{}'", filename));

        struct stat st{};
        if (fstat(info.bsp_file, &st) < 0)
            throw std::runtime_error(std::format("Could not stat file '{}'", filename));

        info.file_size = st.st_size;

        void* data = mmap(nullptr, info.file_size, PROT_READ, MAP_PRIVATE | MAP_FILE, info.bsp_file, 0);
        if (data == MAP_FAILED)
            throw std::runtime_error(std::format("Could not mmap file '{}'", filename));

        if (madvise(data, info.file_size, MADV_RANDOM | MADV_WILLNEED | MADV_HUGEPAGE) < 0)
            throw std::runtime_error(std::format("Could not madvise file '{}'", filename));

        info.file_data = reinterpret_cast<uint8_t*>(data);

#endif


        info.resources = resources;

        parse_header(info);

        //read_map(info);
        read_textures(info);
    }

}