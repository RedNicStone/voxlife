
#include <hl1/read_entities.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vec_swizzle.hpp>

#include <set>
#include <iostream>
#include <charconv>


namespace voxlife::hl1 {

    template<typename T>
    std::from_chars_result from_chars(std::string_view str, T& value) {
        return std::from_chars(str.data(), str.data() + str.size(), value);
    }

    template<>
    std::from_chars_result from_chars(std::string_view str, bool& value) {
        if (str == "0")
            value = false;
        else if (str == "1")
            value = true;
        else
            return { nullptr, std::errc::invalid_argument };

        return { nullptr, std::errc() };
    }

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
            auto from_chars_result = from_chars(x_str, x);
            if (from_chars_result.ec != std::errc()) {
                result = false;
                return;
            }

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

    template<typename T>
    struct function_traits;

    template<typename Ret, typename... Args>
    struct function_traits<Ret(*)(Args...)> {
        using return_type = Ret;
        using args_tuple_type = std::tuple<Args...>;
    };

    template<typename Ret, typename ClassType, typename... Args>
    struct function_traits<Ret(ClassType::*)(Args...)> {
        using return_type = Ret;
        using args_tuple_type = std::tuple<Args...>;
    };

    template<typename Ret, typename ClassType, typename... Args>
    struct function_traits<Ret(ClassType::*)(Args...) const> {
        using return_type = Ret;
        using args_tuple_type = std::tuple<Args...>;
    };

    template<typename T>
    struct function_traits : function_traits<decltype(&T::operator())> {};

    template<typename Func>
    using entity_type = typename std::remove_reference<typename std::tuple_element<0, typename function_traits<Func>::args_tuple_type>::type>::type;

    // Func should have a signature of:
    // void(T&, parameter_type, std::string_view, bool&);
    template<typename Func>
    auto construct_entity(const bsp::entity& entity, Func f) {
        entity_type<Func> result{};

        for (const auto& pair : entity.pairs) {
            auto parameter_type = parameter_type_map.find(pair.key);
            if (parameter_type == parameter_type_map.end()) {
                std::cerr << "Unknown parameter type: " << pair.key << std::endl;
                continue;
            }

            bool parse_result = true;
            f(result, parameter_type->second, pair.key, pair.value, parse_result);

            if (!parse_result) {
                std::cerr << "Failed to parse parameter '" << pair.key << "' with value '" << pair.value << "'"
                          << std::endl;
                break;
            }
        }

        return result;
    }

#define PARAMETER_CONSTRUCTOR(type_name) \
    void construct_parameter_##type_name(entity_types::type_name& result, parameter_type type, std::string_view key, std::string_view value, bool& parse_result)

    PARAMETER_CONSTRUCTOR(light) {
        switch (type) {
            case parameter_type::origin:
                parse_result = tag_values_from_chars(value, result.origin.x, result.origin.y, result.origin.z);
                break;
            case parameter_type::light:
                parse_result = tag_values_from_chars(value, result.color.r, result.color.g, result.color.b, result.intensity);
                if (!parse_result)
                    parse_result = tag_values_from_chars(value, result.color.r, result.color.g, result.color.b);
                break;
            case parameter_type::style:
                if (value != "0" && value != "32" && value != "33")
                    parse_result = false;
                break;
            case parameter_type::fade:
                parse_result = tag_values_from_chars(value, result.fade);
                break;
            case parameter_type::classname:
                break;
            default:
                std::cerr << "Unparsed parameter type: " << key << std::endl;
                break;
        }
    }

    PARAMETER_CONSTRUCTOR(info_player_start) {
        switch (type) {
            case parameter_type::origin:
                parse_result = tag_values_from_chars(value, result.origin.x, result.origin.y, result.origin.z);
                break;
            case parameter_type::angle:
                parse_result = tag_values_from_chars(value, result.angle);
                break;
            case parameter_type::classname:
                break;
            default:
                std::cerr << "Unparsed parameter type: " << key << std::endl;
                break;
        }
    }

    PARAMETER_CONSTRUCTOR(trigger_changelevel) {
        switch (type) {
            case parameter_type::model:
                //parse_result = tag_values_from_chars(pair.value, result.model);
                result.model = value;
                break;
            case parameter_type::landmark:
                result.landmark = value;
                break;
            case parameter_type::map:
                result.map = value;
                break;
            case parameter_type::classname:
                break;
            default:
                std::cerr << "Unparsed parameter type: " << key << std::endl;
                break;
        }
    }

    PARAMETER_CONSTRUCTOR(worldspawn) {
        switch (type) {
            case parameter_type::message:
                result.message = value;
                break;
            case parameter_type::skyname:
                result.skyname = value;
                break;
            case parameter_type::chaptertitle:
                result.chaptertitle = value;
                break;
            case parameter_type::gametitle:
                parse_result = tag_values_from_chars(value, result.gametitle);
                break;
            case parameter_type::newunit:
                parse_result = tag_values_from_chars(value, result.newunit);
                break;
            case parameter_type::wad:
                result.wad = value;
                break;
            default:
                std::cerr << "Unparsed parameter type: " << key << std::endl;
                break;
        }
    }

    entity construct_entity(const bsp::entity& entity, classname_type type) {
        switch (type) {
            case classname_type::light:
                return construct_entity(entity, construct_parameter_light);
            case classname_type::info_player_start:
                return construct_entity(entity, construct_parameter_info_player_start);
            case classname_type::trigger_changelevel:
                return construct_entity(entity, construct_parameter_trigger_changelevel);
            case classname_type::worldspawn:
                return construct_entity(entity, construct_parameter_worldspawn);
            default:
                //std::cerr << "Unable to parse entity type: " << classname_names[static_cast<uint32_t>(type)] << std::endl;
                return std::monostate{};
        }
    }

    level_entities read_entities(bsp::bsp_handle handle) {
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
