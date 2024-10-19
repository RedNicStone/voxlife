
#include <hl1/read_entities.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vec_swizzle.hpp>

#include <set>
#include <iostream>
#include <charconv>


namespace voxlife::hl1 {

    template<typename...Ts>
    bool tag_values_from_chars(std::string_view value_str, Ts&...ts) {
        auto beg = value_str.begin();
        bool result = true;
        int index = 0;
        auto parse_single = [&](auto& x) {
            index += 1;
            bool is_last = index == sizeof...(Ts);
            if (!result)
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

    auto create_entity_type_map() {
        std::unordered_map<std::string_view, classname_type> result;
        for (uint32_t i = 0; i < static_cast<uint32_t>(classname_type::CLASSNAME_TYPE_MAX); ++i)
            result[classname_names[i]] = static_cast<classname_type>(i);

        return result;
    }

    static auto entity_type_map = create_entity_type_map();

    auto create_parameter_type_map() {
        std::unordered_map<std::string_view, parameter_type> result;
        for (uint32_t i = 0; i < static_cast<uint32_t>(parameter_type::PARAMETER_TYPE_MAX); ++i)
            result[parameter_names[i]] = static_cast<parameter_type>(i);

        return result;
    }

    static auto parameter_type_map = create_parameter_type_map();

    entity_types::light construct_light(const bsp::entity& entity) {
        entity_types::light result{};

        for (const auto& pair : entity.pairs) {
            auto parameter_type = parameter_type_map.find(pair.key);
            if (parameter_type == parameter_type_map.end()) {
                std::cerr << "Unknown parameter type: " << pair.key << std::endl;
                continue;
            }

            bool parse_result = true;
            switch (parameter_type->second) {
                case parameter_type::origin:
                    parse_result = tag_values_from_chars(pair.value, result.origin.x, result.origin.y, result.origin.z);
                    break;
                case parameter_type::light:
                    parse_result = tag_values_from_chars(pair.value, result.color.r, result.color.g, result.color.b, result.intensity);
                    if (!parse_result)
                        parse_result = tag_values_from_chars(pair.value, result.color.r, result.color.g, result.color.b);
                    break;
                case parameter_type::style:
                    if (pair.value != "0" && pair.value != "32" && pair.value != "33")
                        parse_result = false;
                    break;
                case parameter_type::fade:
                    parse_result = tag_values_from_chars(pair.value, result.fade);
                    break;
                case parameter_type::classname:
                    break;
                default:
                    std::cerr << "Unparsed parameter type: " << pair.key << std::endl;
                    break;
            }

            if (!parse_result) {
                std::cerr << "Failed to parse parameter '" << pair.key << "' with value '" << pair.value << "'" << std::endl;
                break;
            }
        }

        return result;
    }

    entity_types::info_player_start construct_info_player_start(const bsp::entity& entity) {
        entity_types::info_player_start result{};

        for (const auto& pair : entity.pairs) {
            auto parameter_type = parameter_type_map.find(pair.key);
            if (parameter_type == parameter_type_map.end()) {
                std::cerr << "Unknown parameter type: " << pair.key << std::endl;
                continue;
            }

            bool parse_result = true;
            switch (parameter_type->second) {
                case parameter_type::origin:
                    parse_result = tag_values_from_chars(pair.value, result.origin.x, result.origin.y, result.origin.z);
                    break;
                case parameter_type::angle:
                    parse_result = tag_values_from_chars(pair.value, result.angle);
                    break;
                case parameter_type::classname:
                    break;
                default:
                    std::cerr << "Unparsed parameter type: " << pair.key << std::endl;
                    break;
            }

            if (!parse_result) {
                std::cerr << "Failed to parse parameter '" << pair.key << "' with value '" << pair.value << "'" << std::endl;
                break;
            }
        }

        return result;
    }

    entity_types::trigger_changelevel construct_trigger_changelevel(const bsp::entity& entity) {
        entity_types::trigger_changelevel result{};

        for (const auto& pair : entity.pairs) {
            auto parameter_type = parameter_type_map.find(pair.key);
            if (parameter_type == parameter_type_map.end()) {
                std::cerr << "Unknown parameter type: " << pair.key << std::endl;
                continue;
            }

            bool parse_result = true;
            switch (parameter_type->second) {
                case parameter_type::model:
                    //parse_result = tag_values_from_chars(pair.value, result.model);
                    result.model = pair.value;
                    break;
                case parameter_type::landmark:
                    result.landmark = pair.value;
                    break;
                case parameter_type::map:
                    result.map = pair.value;
                    break;
                case parameter_type::classname:
                    break;
                default:
                    std::cerr << "Unparsed parameter type: " << pair.key << std::endl;
                    break;
            }

            if (!parse_result) {
                std::cerr << "Failed to parse parameter '" << pair.key << "' with value '" << pair.value << "'" << std::endl;
                break;
            }
        }

        return result;
    }

    entity construct_entity(const bsp::entity& entity, classname_type type) {
        switch (type) {
            case classname_type::light:
                return construct_light(entity);
            case classname_type::info_player_start:
                return construct_info_player_start(entity);
            case classname_type::trigger_changelevel:
                return construct_trigger_changelevel(entity);
            default:
                //std::cerr << "Unable to parse entity type: " << classname_names[static_cast<uint32_t>(type)] << std::endl;
                return std::monostate{};
        }
    }

    level_entities read_level(bsp::bsp_handle handle) {
        level_entities result{};

        auto entities = bsp::get_entities(handle);

        for (auto const &entity : entities) {
            auto class_name = std::find_if(entity.pairs.begin(), entity.pairs.end(), [](const bsp::entity::key_value_pair &pair) {
                auto parameter_type = parameter_type_map.find(pair.key);
                return parameter_type != parameter_type_map.end() && parameter_type->second == parameter_type::classname;
            });
            if (class_name == entity.pairs.end()) {
                std::cerr << "Entity has no classname" << std::endl;
                continue;
            }

            std::string_view entity_classname = class_name->value;
            auto class_type = entity_type_map.find(entity_classname);
            if (class_type == entity_type_map.end()) {
                std::cerr << "Unknown entity type: " << entity_classname << std::endl;
                continue;
            }

            auto level_entity = construct_entity(entity, class_type->second);
            if (std::holds_alternative<std::monostate>(level_entity))
                continue;

            result.entities[static_cast<size_t>(class_type->second)].emplace_back(level_entity);
        }

        return result;
    }

}
