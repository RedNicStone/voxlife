
#include <hl1/read_level.h>
#include <hl1/read_entities.h>
#include <bsp/read_file.h>
#include <voxel/write_file.h>
#include <voxel/cooridnates.h>
#include <voxel/voxelize_polygon.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vec_swizzle.hpp>

#include <filesystem>
#include <iostream>
#include <charconv>


using namespace voxlife::voxel;

namespace voxlife::hl1 {
    int load_level(std::string_view game_path, std::string_view level_name) {
        std::filesystem::path game_path_fs(game_path);

        if (!std::filesystem::is_directory(game_path_fs)) {
            std::cerr << "Game path does not point to a valid directory" << std::endl;
            return 1;
        }

        auto level_name_with_ext = std::string(level_name) + ".bsp";
        auto level_path = game_path_fs / "valve" / "maps" / level_name_with_ext;
        if (!std::filesystem::is_regular_file(level_path)) {
            std::cerr << "Could not file level at " << level_path << std::endl;
            return 1;
        }

        voxlife::bsp::bsp_handle bsp_handle;
        voxlife::bsp::open_file(std::filesystem::weakly_canonical(level_path).make_preferred().string(), &bsp_handle);
        auto entities = read_entities(bsp_handle);
        std::vector<wad::wad_handle> wad_handles;

        {
            auto worldspan_entities = entities.entities[static_cast<size_t>(classname_type::worldspawn)];
            if (worldspan_entities.empty())
                std::cerr << "Could not find worldspawn entity" << std::endl;

            auto& worldspawn = std::get<entity_types::worldspawn>(worldspan_entities[0]);

            {
                std::vector<std::string> wad_paths;
                size_t start = 0;
                size_t end;
                while (start <= worldspawn.wad.size()) {
                    end = worldspawn.wad.find(';', start);
                    if (end == std::string_view::npos) {
                        end = worldspawn.wad.size();
                    }
                    std::string_view segment = worldspawn.wad.substr(start, end - start);
                    if (!segment.empty()) {
                        std::string relative_wad_path;
                        relative_wad_path.reserve(segment.size());
                        std::replace_copy(segment.begin(), segment.end(), std::back_inserter(relative_wad_path), '\\', '/');
                        std::filesystem::path absolute_wad_path = game_path_fs;
                        std::filesystem::path relative_wad_path_fs(std::move(relative_wad_path));

                        // std::filesystem doesn't have a way to remove the top-level directory, so we have to do it manually
                        auto it = ++relative_wad_path_fs.begin();
                        it++;
                        for (; it != relative_wad_path_fs.end(); ++it)
                            absolute_wad_path /= *it;

                        wad_paths.push_back(std::filesystem::weakly_canonical(absolute_wad_path).make_preferred().string());
                    }

                    start = end + 1;
                }

                wad_handles.reserve(wad_paths.size());
                for (auto& path : wad_paths) {
                    wad::wad_handle wad_handle;
                    try {
                        wad::open_file(path, &wad_handle);
                    } catch (std::exception& e) {
                        std::cerr << "Failed to open wad file " << path << ": " << e.what() << std::endl;
                    }

                    wad_handles.push_back(wad_handle);
                }

                bsp::load_textures(bsp_handle, wad_handles);
            }

            {
                auto skybox_name = worldspawn.skyname;

                auto texture = bsp::get_texture_data(bsp_handle, skybox_name);
                auto a = 0;
            }
        }

        {
            auto faces = voxlife::bsp::get_model_faces(bsp_handle, 0);
            std::vector<Model> models;
            models.reserve(faces.size());

            uint32_t count = 0;
            for (auto& face : faces) {
                try {
                    models.emplace_back();
                    voxelize_face(bsp_handle, level_name, face, count++, models.back());
                } catch (std::exception& e) {
                    std::cerr << e.what() << std::endl;
                }
            }

            std::vector<Light> lights;
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::light)]) {
                auto& light_entity = std::get<voxlife::hl1::entity_types::light>(entity);
                lights.push_back({
                                         .pos = glm::vec3(glm::xzy(light_entity.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter),
                                         .color = light_entity.color,
                                         .intensity = static_cast<float>(light_entity.intensity) * (hammer_to_teardown_scale * decimeter_to_meter * 20),
                                 });
            }

            std::vector<Location> locations;
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::info_landmark)]) {
                auto& landmark = std::get<voxlife::hl1::entity_types::info_landmark>(entity);
                locations.push_back({
                                            .name = std::string(landmark.targetname),
                                            .pos = glm::vec3(glm::xzy(landmark.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter),
                                    });
            }

            auto& player_start_entities = entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::info_player_start)];
            if (player_start_entities.empty())
                throw std::runtime_error("Could not find player start");

            auto& player_start = std::get<voxlife::hl1::entity_types::info_player_start>(player_start_entities.front());

            std::vector<Trigger> triggers;
            triggers.reserve(entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::trigger_changelevel)].size());
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::trigger_changelevel)]) {
                auto& transition = std::get<voxlife::hl1::entity_types::trigger_changelevel>(entity);
                if (transition.model[0] != '*') {
                    std::cerr << "Level transition trigger is an external model, skipping" << std::endl;
                    continue;
                }

                auto model_id_string = transition.model.substr(1);
                uint32_t model_id;
                auto result = std::from_chars(model_id_string.data(), model_id_string.data() + model_id_string.size(), model_id);
                if (result.ec != std::errc() || model_id == 0) {
                    std::cerr << "Failed to parse model id from '" << transition.model << "'" << std::endl;
                    continue;
                }

                auto model_aabb = bsp::get_model_aabb(bsp_handle, model_id);
                model_aabb.min = glm::vec3(glm::xzy(model_aabb.min)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
                model_aabb.max = glm::vec3(glm::xzy(model_aabb.max)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
                // Note: Necessary because the z axis is flipped
                std::swap(model_aabb.min.z, model_aabb.max.z);

                triggers.push_back(Trigger{
                    .map = std::string(transition.map),
                    .landmark = std::string(transition.landmark),
                    .pos = model_aabb.min,
                    .size = model_aabb.max - model_aabb.min,
                });
            }

            LevelInfo info;
            info.name = level_name;
            info.models = models;
            info.lights = lights;
            info.locations = locations;
            info.triggers = triggers;
            info.spawn_pos = glm::vec3(glm::xzy(player_start.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
            info.spawn_rot = glm::vec3(0, player_start.angle + 90, 0);

            auto level_aabb = bsp::get_model_aabb(bsp_handle, 0);

            level_aabb.min = glm::vec3(glm::xzy(level_aabb.min)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
            level_aabb.max = glm::vec3(glm::xzy(level_aabb.max)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
            // Note: Necessary because the z axis is flipped
            std::swap(level_aabb.min.z, level_aabb.max.z);

            info.level_pos = glm::vec3(0, 128, 0) - level_aabb.min - (level_aabb.max.z - level_aabb.min.z) * 0.5f;

            write_teardown_level(info);
        }

        for (auto wad_handle : wad_handles) {
            voxlife::wad::release(wad_handle);
        }
        wad_handles.clear();
        voxlife::bsp::release(bsp_handle);

        return 0;
    }

}
