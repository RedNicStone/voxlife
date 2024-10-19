
#include <bsp/readfile.h>
#include <bsp/primitives.h>

#include <stdexcept>
#include <format>
#include <iostream>
#include <fstream>
#include <span>
#include <charconv>

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

        std::vector<lump_entity>           entities;
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

        info.entities_str  = std::string_view(reinterpret_cast<const char*>      (info.lump_begins[lump_type::LUMP_ENTITIES]),
                                       reinterpret_cast<const char*>             (  info.lump_ends[lump_type::LUMP_ENTITIES]));
        info.planes        = std::span(reinterpret_cast<const lump_plane*>       (info.lump_begins[lump_type::LUMP_PLANES]),
                                       reinterpret_cast<const lump_plane*>       (  info.lump_ends[lump_type::LUMP_PLANES]));
        info.textures      = std::span(reinterpret_cast<const lump_mip_texture*> (info.lump_begins[lump_type::LUMP_TEXTURES]),
                                       reinterpret_cast<const lump_mip_texture*> (  info.lump_ends[lump_type::LUMP_TEXTURES]));
        info.vertices      = std::span(reinterpret_cast<const lump_vertex*>      (info.lump_begins[lump_type::LUMP_VERTICES]),
                                       reinterpret_cast<const lump_vertex*>      (  info.lump_ends[lump_type::LUMP_VERTICES]));
        info.nodes         = std::span(reinterpret_cast<const lump_node*>        (info.lump_begins[lump_type::LUMP_NODES]),
                                       reinterpret_cast<const lump_node*>        (  info.lump_ends[lump_type::LUMP_NODES]));
        info.texture_infos = std::span(reinterpret_cast<const lump_texture_info*>(info.lump_begins[lump_type::LUMP_TEXINFO]),
                                       reinterpret_cast<const lump_texture_info*>(  info.lump_ends[lump_type::LUMP_TEXINFO]));
        info.faces         = std::span(reinterpret_cast<const lump_face*>        (info.lump_begins[lump_type::LUMP_FACES]),
                                       reinterpret_cast<const lump_face*>        (  info.lump_ends[lump_type::LUMP_FACES]));
        info.clip_nodes    = std::span(reinterpret_cast<const lump_clip_node*>   (info.lump_begins[lump_type::LUMP_CLIPNODES]),
                                       reinterpret_cast<const lump_clip_node*>   (  info.lump_ends[lump_type::LUMP_CLIPNODES]));
        info.leafs         = std::span(reinterpret_cast<const lump_leaf*>        (info.lump_begins[lump_type::LUMP_LEAFS]),
                                       reinterpret_cast<const lump_leaf*>        (  info.lump_ends[lump_type::LUMP_LEAFS]));
        info.mark_surfaces = std::span(reinterpret_cast<const lump_mark_surface*>(info.lump_begins[lump_type::LUMP_MARKSURFACES]),
                                       reinterpret_cast<const lump_mark_surface*>(  info.lump_ends[lump_type::LUMP_MARKSURFACES]));
        info.edges         = std::span(reinterpret_cast<const lump_edge*>        (info.lump_begins[lump_type::LUMP_EDGES]),
                                       reinterpret_cast<const lump_edge*>        (  info.lump_ends[lump_type::LUMP_EDGES]));
        info.surface_edges = std::span(reinterpret_cast<const lump_surf_edge*>   (info.lump_begins[lump_type::LUMP_SURFEDGES]),
                                       reinterpret_cast<const lump_surf_edge*>   (  info.lump_ends[lump_type::LUMP_SURFEDGES]));
        info.models        = std::span(reinterpret_cast<const lump_model*>       (info.lump_begins[lump_type::LUMP_MODELS]),
                                       reinterpret_cast<const lump_model*>       (  info.lump_ends[lump_type::LUMP_MODELS]));

        auto begin = info.entities_str.begin();
        while (true) {
            if (begin == info.entities_str.end())
                break;
            auto entity_beg = std::find(begin, info.entities_str.end(), '{');
            if (entity_beg == info.entities_str.end())
                break;
            auto entity_end = std::find(begin, info.entities_str.end(), '}');
            if (entity_end == info.entities_str.end())
                break;
            begin = entity_end + 1;

            auto line_beg = std::find(entity_beg, entity_end, '\n');
            if (line_beg == entity_end)
                break;
            line_beg += 1;

            info.entities.emplace_back();
            auto& entity = info.entities.back();
            while (true) {
                auto line_end = std::find(line_beg, entity_end, '\n');
                if (line_end == entity_end)
                    break;
                auto line = std::string_view(line_beg, line_end + 1);
                line_beg = line_end + 1;

                auto tag_beg = std::find(line.begin(), line.end(), '\"');
                if (tag_beg == line.end())
                    continue;
                auto tag_end = std::find(tag_beg + 1, line.end(), '\"');
                if (tag_end == line.end())
                    continue;
                auto val_beg = std::find(tag_end + 1, line.end(), '\"');
                if (val_beg == line.end())
                    continue;
                auto val_end = std::find(val_beg + 1, line.end(), '\"');
                if (val_end == line.end())
                    continue;

                auto tag_str = std::string_view(tag_beg + 1, tag_end);
                auto val_str = std::string_view(val_beg + 1, val_end);
                if (tag_str == "classname") {
                    entity.classname = val_str;
                } else {
                    entity.key_value.push_back({
                        .key = tag_str,
                        .value = val_str,
                    });
                }
            }
        }
    }

    template<typename T>
    constexpr T& span_at(std::span<T> span, size_t index) {
        if (index >= span.size()) [[unlikely]]
            throw std::out_of_range("Span index out of bounds");

        return span[index];
    }

    template<typename T>
    constexpr std::span<T> safe_subspan(std::span<T> span, size_t offset, size_t length) {
        if (offset + length > span.size()) [[unlikely]]
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

        auto* texture_header = reinterpret_cast<const lump_texture_header*>(texture_lump_begin);
        uint32_t texture_header_length = sizeof(lump_texture_header) + (texture_header->mip_texture_count - 1) * sizeof(uint32_t);
        auto* texture_begin = texture_lump_begin + sizeof(lump_texture_header);
        auto* texture_end = texture_lump_begin + texture_header_length;

        if (texture_end > texture_lump_end)
            throw std::runtime_error("Texture header extends beyond end of lump");

        for (; texture_begin < texture_end; texture_begin += sizeof(uint32_t)) {
            auto texture_offset = *reinterpret_cast<const uint32_t*>(texture_begin);
            auto* mip_texture = reinterpret_cast<const uint8_t*>(texture_lump_begin + texture_offset);
            auto* mip_texture_handle = reinterpret_cast<const lump_mip_texture*>(mip_texture);

            if (reinterpret_cast<const uint8_t*>(mip_texture) > texture_lump_end)
                throw std::runtime_error("Mip texture extends beyond end of lump");

            auto texture_data_end = texture_lump_end;
            if (mip_texture_handle->offsets[0] & mip_texture_handle->offsets[1]
                & mip_texture_handle->offsets[2] & mip_texture_handle->offsets[3]) {
                // Texture is internal, do nothing
            } else {
                // Texture is external, load it
                mip_texture = reinterpret_cast<const uint8_t*>(wad::get_entry(info.resources, mip_texture_handle->name));
                if (mip_texture == nullptr) {
                    //throw std::runtime_error(std::format("Could not find texture '{}'", mip_texture_handle->name));
                    std::cout << std::format("Could not find texture '{}'\n", mip_texture_handle->name);
                    continue;
                }

                size_t texture_size = wad::get_entry_size(info.resources, mip_texture_handle->name);
                texture_data_end = mip_texture + texture_size;

                mip_texture_handle = reinterpret_cast<const lump_mip_texture*>(mip_texture);
            }

            const uint32_t texel_count = mip_texture_handle->width * mip_texture_handle->height;
            auto color_data = mip_texture + mip_texture_handle->offsets[3] + texel_count / 64 + 2;

            if (color_data + 256 > texture_data_end)
                throw std::runtime_error("Color data extends beyond end of lump");

            const uint32_t texture_width  = mip_texture_handle->width;
            const uint32_t texture_height = mip_texture_handle->height;
            const uint8_t* texture_data = mip_texture + mip_texture_handle->offsets[0];

            if (texture_data + texel_count > color_data)
                throw std::runtime_error("Texture data extends beyond color data");

            std::vector<glm::u8vec3> texture_data_vec(texel_count);

            for (uint32_t y = 0; y < texture_height; ++y) {
                for (uint32_t x = 0; x < texture_width; ++x) {
                    uint32_t pixel_index = y * texture_width + x;
                    uint8_t index = texture_data[pixel_index];
                    glm::u8vec3 texel;
                    texel.r = color_data[index * 3 + 0];
                    texel.g = color_data[index * 3 + 1];
                    texel.b = color_data[index * 3 + 2];
                    texture_data_vec[pixel_index] = texel;
                }
            }

            info.loaded_textures.emplace_back(std::move(texture_data_vec), glm::u32vec2(texture_width, texture_height));

            /*
            for (int i = 0; i < lump_mip_texture::mip_levels; ++i) {
                const uint32_t texture_width  = mip_texture_handle->width >> (i * 2);
                const uint32_t texture_height = mip_texture_handle->height >> (i * 2);
                const uint32_t texture_length = texel_count >> (i * 2);
                const uint8_t* texture_data = mip_texture + mip_texture_handle->offsets[i];

                if (texture_data + texture_length > color_data)
                    throw std::runtime_error("Texture data extends beyond color data");

                auto file = std::ofstream(std::format("textures/texture_{}_{}.ppm", a, i), std::ios::out);
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
             */
        }
    }

    texture get_texture_data(bsp_handle handle, uint32_t texture_id) {
        auto& info = *reinterpret_cast<bsp_info*>(handle);

        if (texture_id < info.loaded_textures.size()) {
            auto& texture = info.loaded_textures[texture_id];
            return { texture.data, texture.size };
        }

        auto& texture = info.loaded_textures.front();
        return { texture.data, texture.size };
    }

    template<typename...Ts>
    bool tag_values_from_chars(std::string_view value_str, Ts&...ts) {
        auto beg = value_str.begin();
        bool result = true;
        int index = 0;
        auto parse_single = [&](auto& x) {
            index += 1;
            bool is_last = index == sizeof...(Ts);
            if (result == false)
                return;
            auto tag_end = std::find(beg, value_str.end(), ' ');
            if (tag_end == value_str.end() && !is_last) {
                result = false;
                return;
            }
            auto x_str = std::string_view(beg, tag_end);
            std::from_chars(x_str.data(), x_str.data() + x_str.size(), x);
            if (!is_last)
                beg = tag_end + 1;
        };

        (parse_single(ts), ...);

        return result;
    }

    entities get_entities(bsp_handle handle) {
        auto& info = *reinterpret_cast<bsp_info*>(handle);

        entities result{};
        bool seen_player_start = false;

        for (auto const &entity : info.entities) {
            bool parse_result = true;

            if (entity.classname == "light") {
                light light;
                light.color = {};
                light.intensity = {};
                light.origin = {};
                light.fade = 1.0f;

                for (auto const &[key, value] : entity.key_value) {
                    if (key == "origin") {
                        parse_result &= tag_values_from_chars(value, light.origin.x, light.origin.y, light.origin.z);
                    } else if (key == "_light") {
                        if (!tag_values_from_chars(value, light.color.r, light.color.g, light.color.b, light.intensity)) {
                            parse_result &= tag_values_from_chars(value, light.color.r, light.color.g, light.color.b);
                            light.intensity = 255; // ?
                        }
                    } else if (key == "style") {
                        if (value != "0" && value != "32" && value != "33") {
                            parse_result = false;
                        }
                    } else if (key == "_fade") {
                        parse_result &= tag_values_from_chars(value, light.fade);
                    }
                    // targetname, spawnflags...?

                    if (!parse_result)
                        break;
                }

                if (parse_result) {
                    result.lights.push_back(light);
                }
            } else if (entity.classname == "info_player_start") {
                if (seen_player_start) {
                    // ignore all but first
                    continue;
                }
                seen_player_start = true;
                for (auto const &[key, value] : entity.key_value) {
                    if (key == "origin") {
                        parse_result &= tag_values_from_chars(value, result.player_start.origin.x, result.player_start.origin.y, result.player_start.origin.z);
                    } else if (key == "angle") {
                        parse_result &= tag_values_from_chars(value, result.player_start.angle);
                    }
                    // spawnflags...?
                }
            } else if (entity.classname == "trigger_changelevel") {
                trigger_changelevel changelevel;
                for (auto const &[key, value] : entity.key_value) {
                    if (key == "model") {
                        parse_result &= tag_values_from_chars(value, changelevel.model_id);
                    } else if (key == "landmark") {
                        changelevel.landmark = value;
                    } else if (key == "map") {
                        changelevel.map = value;
                    }
                }
                result.changelevels.push_back(changelevel);
            } else if (entity.classname == "info_landmark") {
                glm::ivec3 origin;
                std::string_view targetname;
                for (auto const &[key, value] : entity.key_value) {
                    if (key == "origin") {
                        parse_result &= tag_values_from_chars(value, origin.x, origin.y, origin.z);
                    } else if (key == "targetname") {
                        targetname = value;
                    }
                }
                result.landmarks.emplace_back(targetname, origin);
            }

            if (!parse_result) {
                std::cout << std::format("---failed to parse {}---\n", entity.classname);
                for (auto const &[key, value] : entity.key_value) {
                    std::cout << key << " = " << value << "\n";
                }
            } else {
                // std::cout << entity.classname << ":" << std::endl;
                // for (auto const &[key, value] : entity.key_value) {
                //     std::cout << "  - " << key << " = " << value << "\n";
                // }
            }
        }
        return result;
    }

    std::vector<face> get_model_faces(bsp_handle handle, uint32_t model_id) {
        auto& info = *reinterpret_cast<bsp_info*>(handle);

        const auto& root_model = span_at(info.models, model_id);
        const auto& root_node = span_at(info.nodes, root_model.head_nodes[0]);

        std::vector<lump_face> bsp_faces;
        auto face_span = safe_subspan(info.faces, root_model.first_face, root_model.face_count);
        bsp_faces.insert(bsp_faces.end(), face_span.begin(), face_span.end());

        /*
        std::stack<lump_node> node_stack;
        node_stack.push(root_node);

        uint32_t node_count = 0;
        uint32_t leaf_count = 0;
        uint32_t face_count = 0;

        while (!node_stack.empty()) {
            auto &node = node_stack.top();
            node_stack.pop();

            auto node_face_span = safe_subspan(info.faces, node.first_face, node.face_count);
            for (auto& face : node_face_span)
                bsp_faces.push_back(face);

            for (auto& child_node : node.children) {
                std::span<const lump_mark_surface> mark_surfaces;

                if (child_node > 0) {
                    node_count++;
                    node_stack.push(span_at(info.nodes, child_node));
                    continue;
                }

                leaf_count++;
                auto& leaf = span_at(info.leafs, -child_node - 1);
                mark_surfaces = safe_subspan(info.mark_surfaces, leaf.first_mark_surface, leaf.mark_surface_count);

                face_count += leaf.mark_surface_count;

                for (auto& mark_surface : mark_surfaces) {
                    auto &face = span_at(info.faces, mark_surface.face);

                    bsp_faces.push_back(face);
                }
            }
        }*/

        std::vector<face> faces;

        for (auto& face : bsp_faces) {
            auto& plane = span_at(info.planes, face.plane);

            std::vector<glm::vec3> vertices;
            auto surface_edges = safe_subspan(info.surface_edges, face.first_edge, face.edge_count);

            for (auto& surface_edge : surface_edges) {
                auto edge = span_at(info.edges, std::abs(surface_edge.edge));
                uint16_t vertex_index = surface_edge.edge < 0 ? edge.vertex[0] : edge.vertex[1];

                auto& vertex = span_at(info.vertices, vertex_index);
                vertices.push_back(vertex);
            }

            auto& texture_info = span_at(info.texture_infos, face.texture_info);
            glm::vec<2, face::texture_position> texture_coords{};
            texture_coords.x.axis = texture_info.s;
            texture_coords.x.shift = texture_info.shift_s;
            texture_coords.y.axis = texture_info.t;
            texture_coords.y.shift = texture_info.shift_t;

            faces.emplace_back(
                    static_cast<face::type>(plane.type),
                    texture_coords,
                    texture_info.mip_texture,
                    plane.normal,
                    std::move(vertices)
                    );
        }

        return faces;
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
        read_textures(info);

        //read_map(info);
    }

}