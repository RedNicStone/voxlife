
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
        voxlife::bsp::open_file(level_path.c_str(), &bsp_handle);
        auto entities = read_entities(bsp_handle);

        {
            auto worldspan_entities = entities.entities[static_cast<size_t>(classname_type::worldspawn)];
            if (worldspan_entities.empty())
                std::cerr << "Could not find worldspawn entity" << std::endl;

            auto& worldspawn = std::get<entity_types::worldspawn>(worldspan_entities[0]);

            std::vector<std::filesystem::path> wad_paths;
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

                    wad_paths.push_back(absolute_wad_path);
                }

                start = end + 1;
            }

            std::vector<wad::wad_handle> wad_handles;
            wad_handles.reserve(wad_paths.size());
            for (auto& path : wad_paths) {
                wad::wad_handle wad_handle;
                try {
                    wad::open_file(path.c_str(), &wad_handle);
                } catch (std::exception& e) {
                    std::cerr << "Failed to open wad file " << path << ": " << e.what() << std::endl;
                }

                wad_handles.push_back(wad_handle);
            }

            bsp::load_textures(bsp_handle, wad_handles);
        }

        {
            auto faces = voxlife::bsp::get_model_faces(bsp_handle, 0);
            std::vector<Model> models;
            models.reserve(faces.size());

            uint32_t count = 0;
            for (auto& face : faces) {
                try {
                    models.emplace_back();
                    voxelize_face(bsp_handle, face, count++, models.back());
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

            std::vector<bsp::aabb> level_transitions;
            level_transitions.reserve(entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::trigger_changelevel)].size());
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::trigger_changelevel)]) {
                auto& transition = std::get<voxlife::hl1::entity_types::trigger_changelevel>(entity);
                if (transition.model[0] != '*') {
                    std::cerr << "Level transition trigger is an external model, skipping" << std::endl;
                    continue;
                }

                auto model_id_string = transition.map.substr(1);
                uint32_t model_id;
                auto result = std::from_chars(model_id_string.data(), model_id_string.data() + model_id_string.size(), model_id);
                if (result.ec != std::errc()) {
                    std::cerr << "Failed to parse model id from '" << model_id_string << "'" << std::endl;
                    continue;
                }

                auto model_aabb = bsp::get_model_aabb(bsp_handle, model_id);
                model_aabb.min = glm::vec3(glm::xzy(model_aabb.min)) * (hammer_to_teardown_scale * decimeter_to_meter);
                model_aabb.max = glm::vec3(glm::xzy(model_aabb.max)) * (hammer_to_teardown_scale * decimeter_to_meter);
                model_aabb.min.z *= -1.0f;
                model_aabb.max.z *= -1.0f;
                level_transitions.push_back(model_aabb);
            }

            LevelInfo info;
            info.name = "test";
            info.models = models;
            info.lights = lights;
            info.locations = locations;
            info.spawn_pos = glm::vec3(glm::xzy(player_start.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
            info.spawn_rot = glm::vec3(0, player_start.angle + 90, 0);

            write_teardown_level(info);
        }

        return 0;
    }

}
