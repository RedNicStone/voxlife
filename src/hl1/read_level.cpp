
#include <hl1/read_level.h>
#include <hl1/read_entities.h>
#include <bsp/read_file.h>
#include <set>
#include <voxel/write_file.h>
#include <voxel/cooridnates.h>
#include <voxel/voxelize_polygon.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <filesystem>
#include <iostream>
#include <charconv>
#include <ranges>

#include <voxel/test.h>

using namespace voxlife::voxel;

namespace voxlife::hl1 {

    std::array<const std::string_view, 96> default_level_names = {
        // Black Mesa Inbound
        "c0a0",
        "c0a0a",
        "c0a0b",
        "c0a0c",
        "c0a0d",
        "c0a0e",
        // Anomalous Materials
        "c1a0",
        "c1a0d",
        "c1a0a",
        "c1a0b",
        "c1a0e",
        // Unforseen Consequences
        "c1a0c",
        "c1a1",
        "c1a1a",
        "c1a1f",
        "c1a1b",
        "c1a1c",
        "c1a1d",
        // Office Complex
        "c1a2",
        "c1a2a",
        "c1a2b",
        "c1a2c",
        "c1a2d",
        // We've Got Hostiles
        "c1a3",
        "c1a3d",
        "c1a3a",
        "c1a3b",
        "c1a3c",
        // Blast Pit
        "c1a4",
        "c1a4k",
        "c1a4b",
        "c1a4d",
        "c1a4e",
        "c1a4f",
        "c1a4i",
        "c1a4g",
        "c1a4j",
        // Power Up
        "c2a1",
        "c2a1b",
        "c2a1a",
        // On A Rail
        "c2a2",
        "c2a2a",
        "c2a2b1",
        "c2a2b2",
        "c2a2c",
        "c2a2d",
        "c2a2e",
        "c2a2f",
        "c2a2g",
        "c2a2h",
        // Apprehension
        "c2a3",
        "c2a3a",
        "c2a3b",
        "c2a3c",
        "c2a3d",
        "c2a3e",
        // Residue Processing
        "c2a4",
        "c2a4a",
        "c2a4b",
        "c2a4c",
        // Questionable Ethics
        "c2a4d",
        "c2a4e",
        "c2a4f",
        "c2a4g",
        // Surface Tension
        "c2a5",
        "c2a5w",
        "c2a5x",
        "c2a5a",
        "c2a5b",
        "c2a5c",
        "c2a5d",
        "c2a5e",
        "c2a5f",
        "c2a5g",
        // Forget About Freeman
        "c3a1",
        "c3a1a",
        "c3a1b",
        // Lambda Core
        "c3a2e",
        "c3a2",
        "c3a2a",
        "c3a2b",
        "c3a2c",
        "c3a2d",
        "c3a2f",
        // Xen
        "c4a1",
        // Gonarch's Lair
        "c4a2",
        "c4a2a",
        "c4a2b",
        // Interloper
        "c4a1a",
        "c4a1b",
        "c4a1c",
        "c4a1d",
        "c4a1e",
        // Nihilanth
        "c4a1f",
        "c4a3",
        "c5a1",
    };

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

            auto &worldspawn = std::get<entity_types::worldspawn>(worldspan_entities[0]);

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

                        auto it = relative_wad_path_fs.begin();
                        if (relative_wad_path_fs.has_parent_path()) {
                            // std::filesystem doesn't have a way to remove the top-level directory, so we have to do it manually
                            // Hack because the wad path sometimes has garbage
                            it++;
                            it++;
                        } else {
                            relative_wad_path_fs = "valve" / relative_wad_path_fs;
                            it = relative_wad_path_fs.begin();
                        }
                        for (; it != relative_wad_path_fs.end(); ++it)
                            absolute_wad_path /= *it;

                        wad_paths.push_back(std::filesystem::weakly_canonical(absolute_wad_path).make_preferred().string());
                    }

                    start = end + 1;
                }

                wad_handles.reserve(wad_paths.size());
                for (auto &path : wad_paths) {
                    wad::wad_handle wad_handle;
                    try {
                        wad::open_file(path, &wad_handle);
                    } catch (std::exception &e) {
                        std::cerr << "Failed to open wad file " << path << ": " << e.what() << std::endl;
                    }

                    wad_handles.push_back(wad_handle);
                }

                bsp::load_textures(bsp_handle, wad_handles);
            }
        }

        // test_voxelization_gui(bsp_handle);

        if (true) {
            std::vector<Model> models;

            bool has_sky = false;
            auto faces = voxlife::bsp::get_model_faces(bsp_handle, 0);
            for (auto &face : faces) {
                auto texture_name = voxlife::bsp::get_texture_name(bsp_handle, face.texture_id);
                if (texture_name == "SKY" || texture_name == "sky") {
                    has_sky = true;
                    break;
                }
            }

            if (false) {
                auto faces = voxlife::bsp::get_model_faces(bsp_handle, 0);
                models.reserve(faces.size());
                uint32_t count = 0;
                for (auto &face : faces) {
                    try {
                        auto texture_name = voxlife::bsp::get_texture_name(bsp_handle, face.texture_id);
                        if (texture_name != "SKY" && texture_name != "sky") {
                            models.emplace_back();
                            voxelize_face(bsp_handle, level_name, face, count++, models.back());
                        }
                    } catch (std::exception &e) {
                        std::cerr << e.what() << std::endl;
                    }
                }
            } else {
                voxelize_gpu(bsp_handle, level_name, models);
            }

            std::vector<Light> lights;
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::light)]) {
                auto &light_entity = std::get<voxlife::hl1::entity_types::light>(entity);
                lights.push_back({
                    .pos = glm::vec3(glm::xzy(light_entity.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter),
                    .color = light_entity.color,
                    .intensity = static_cast<float>(light_entity.intensity) * (hammer_to_teardown_scale * decimeter_to_meter * 20),
                });
            }

            std::vector<Location> locations;
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::info_landmark)]) {
                auto &landmark = std::get<voxlife::hl1::entity_types::info_landmark>(entity);
                locations.push_back({
                    .name = std::string(landmark.targetname),
                    .pos = glm::vec3(glm::xzy(landmark.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter),
                });
            }

            auto &player_start_entities = entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::info_player_start)];

            auto &worldspawn_entities = entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::worldspawn)];

            auto &light_env_entities = entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::light_environment)];

            std::vector<Trigger> triggers;
            triggers.reserve(entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::trigger_changelevel)].size());
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::trigger_changelevel)]) {
                auto &transition = std::get<voxlife::hl1::entity_types::trigger_changelevel>(entity);
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

            if (!player_start_entities.empty()) {
                auto &player_start = std::get<voxlife::hl1::entity_types::info_player_start>(player_start_entities.front());
                info.spawn_pos = glm::vec3(glm::xzy(player_start.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
                info.spawn_rot = glm::vec3(0, player_start.angle + 90, 0);
            }

            auto level_aabb = bsp::get_model_aabb(bsp_handle, 0);
            level_aabb.min = glm::vec3(glm::xzy(level_aabb.min)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
            level_aabb.max = glm::vec3(glm::xzy(level_aabb.max)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
            // Note: Necessary because the z axis is flipped
            std::swap(level_aabb.min.z, level_aabb.max.z);

            info.level_pos = glm::vec3(0, 128, 0) - level_aabb.min - (level_aabb.max.z - level_aabb.min.z) * 0.5f;

            if (!worldspawn_entities.empty() && !light_env_entities.empty() && has_sky) {
                auto &worldspawn = std::get<voxlife::hl1::entity_types::worldspawn>(worldspawn_entities.front());
                auto &light_env = std::get<voxlife::hl1::entity_types::light_environment>(light_env_entities.front());

                info.environment.skybox = std::format("MOD/skyboxes/{}.dds", worldspawn.skyname);
                info.environment.brightness = float(light_env.light_intensity) * 0.1f;
                info.environment.sun_color = glm::vec3(light_env.light_color) / 255.0f;
                auto pitch = light_env.pitch == std::numeric_limits<float>::max() ? light_env.angle_pitch : light_env.pitch;
                auto sun_dir = glm::vec3(0, 0, 1);
                sun_dir = glm::rotateX(sun_dir, glm::radians(pitch));
                sun_dir = glm::rotateY(sun_dir, glm::radians(light_env.angle_yaw));
                info.environment.sun_dir = sun_dir * glm::vec3(1, 1, 1);
            } else {
                info.environment.skybox = "cloudy.dds";
                info.environment.brightness = 0.5f;
                info.environment.sun_color = glm::vec3(0);
                info.environment.sun_dir = glm::vec3(0, -1, 0);
            }

            std::vector<Npc> npcs;
            auto total_npc_count = size_t{0};
            total_npc_count += entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::monster_scientist)].size();
            total_npc_count += entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::monster_barney)].size();
            npcs.reserve(total_npc_count);
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::monster_scientist)]) {
                auto &scientist = std::get<voxlife::hl1::entity_types::monster_scientist>(entity);
                auto pos = glm::vec3(glm::xzy(scientist.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
                auto &npc = npcs.emplace_back();
                npc.pos = pos;
                npc.rot = glm::vec3(0, scientist.angle + 90, 0);
                switch (scientist.body) {
                case -1: npc.path_name = "scientists/prefab-nerd"; break; // random
                case 0: npc.path_name = "scientists/prefab-nerd"; break;
                case 1: npc.path_name = "scientists/prefab-einstein"; break;
                case 2: npc.path_name = "scientists/prefab-luther"; break;
                case 3: npc.path_name = "scientists/prefab-slick"; break;
                default: continue;
                }
            }
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::monster_barney)]) {
                auto &monster = std::get<voxlife::hl1::entity_types::monster_barney>(entity);
                auto pos = glm::vec3(glm::xzy(monster.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
                auto rot = glm::vec3(0, monster.angle + 90, 0);
                npcs.push_back(Npc{.path_name = "barney/prefab", .pos = pos, .rot = rot});
            }
            for (auto const &entity : entities.entities[static_cast<uint32_t>(voxlife::hl1::classname_type::monster_gman)]) {
                auto &monster = std::get<voxlife::hl1::entity_types::monster_gman>(entity);
                auto pos = glm::vec3(glm::xzy(monster.origin)) * glm::vec3(1, 1, -1) * (hammer_to_teardown_scale * decimeter_to_meter);
                auto rot = glm::vec3(0, monster.angle + 90, 0);
                npcs.push_back(Npc{.path_name = "gman/prefab", .pos = pos, .rot = rot});
            }
            info.npcs = npcs;

            // blast-pit side levels broken?
            // c2a2c is outside the shadow volume
            // c2a2g wrong sky
            // xen level transitions have weird scripted teleports?

            write_teardown_level(info);
        }

        for (auto wad_handle : wad_handles) {
            voxlife::wad::release(wad_handle);
        }
        wad_handles.clear();
        voxlife::bsp::release(bsp_handle);

        return 0;
    }

    int load_game_levels(std::string_view game_path, std::span<const std::string_view> level_names) {
        if (level_names.empty())
            level_names = default_level_names;

        for (auto level_name : level_names) {
            std::cout << level_name << std::endl;
            auto result = load_level(game_path, level_name);
            if (result != 0)
                return result;
        }

        return 0;
    }

} // namespace voxlife::hl1
