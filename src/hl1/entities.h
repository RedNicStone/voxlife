//
// Created by RedNicStone on 15/10/24.
//

#ifndef VOXLIFE_ENTITIES_H
#define VOXLIFE_ENTITIES_H

#include <string_view>
#include <vector>
#include <variant>
#include <glm/vec3.hpp>


namespace voxlife::hl1 {

    enum class classname_type {
        ambient_generic,

        ammo_357,
        ammo_9mmAR,
        ammo_9mmbox,
        ammo_9mmclip,
        ammo_ARgrenades,
        ammo_buckshot,
        ammo_crossbow,
        ammo_gaussclip,
        ammo_rpgclip,

        button_target,

        cycler,
        cycler_sprite,
        cycler_wreckage,
        cycler_weapon,

        env_beam,
        env_beverage,
        env_blood,
        env_bubbles,
        env_explosion,
        env_fade,
        env_funnel,
        env_glow,
        env_global,
        env_laser,
        env_message,
        env_rain,
        env_render,
        env_shake,
        env_shooter,
        env_smoker,
        env_snow,
        env_sound,
        env_spark,
        env_sprite,
        env_fog,

        func_breakable,
        func_button,
        func_conveyor,
        func_door,
        func_door_rotating,
        func_friction,
        func_guntarget,
        func_healthcharger,
        func_illusionary,
        func_ladder,
        func_monsterclip,
        func_mortar_field,
        func_pendulum,
        func_plat,
        func_platrot,
        func_pushable,
        func_recharge,
        func_rot_button,
        func_rotating,
        func_tank,
        func_tankcontrols,
        func_tanklaser,
        func_tankmortar,
        func_tankrocket,
        func_trackautochange,
        func_trackchange,
        func_tracktrain,
        func_train,
        func_traincontrols,
        func_wall,
        func_wall_toggle,
        func_water,

        game_counter,
        game_counter_set,
        game_end,
        game_player_equip,
        game_player_hurt,
        game_player_team,
        game_score,
        game_team_master,
        game_team_set,
        game_text,
        game_zone_player,

        gibshooter,

        info_bigmomma,
        info_intermission,
        info_landmark,
        info_node,
        info_node_air,
        info_null,
        info_player_coop,
        info_player_deathmatch,
        info_player_start,
        info_target,
        info_teleport_destination,
        info_texlights,
        infodecal,

        item_airtank,
        item_antidote,
        item_battery,
        item_healthkit,
        item_longjump,
        item_security,
        item_suit,
        world_items,

        light,
        light_environment,
        light_spot,

        momentary_door,
        momentary_rot_button,

        monster_alien_controller,
        monster_alien_grunt,
        monster_alien_slave,
        monster_apache,
        monster_barnacle,
        monster_babycrab,
        monster_barney,
        monster_barney_dead,
        monster_bigmomma,
        monster_bullchicken,
        monster_cockroach,
        monster_flyer_flock,
        monster_furniture,
        monster_gargantua,
        monster_generic,
        monster_gman,
        monster_grunt_repel,
        monster_handgrenade,
        monster_headcrab,
        monster_hevsuit_dead,
        monster_hgrunt_dead,
        monster_houndeye,
        monster_human_assassin,
        monster_human_grunt,
        monster_ichthyosaur,
        monster_leech,
        monster_miniturret,
        monster_nihilanth,
        monster_osprey,
        monster_satchelcharge,
        monster_scientist,
        monster_scientist_dead,
        monster_sentry,
        monster_sitting_scientist,
        monster_snark,
        monster_tentacle,
        monster_tripmine,
        monster_turret,
        monster_zombie,
        monstermaker,

        multi_manager,
        multisource,

        path_corner,
        path_track,
        player_loadsaved,
        player_weaponstrip,

        scripted_sentence,
        scripted_sequence,
        aiscripted_sequence,

        speaker,

        target_cdaudio,

        trigger_auto,
        trigger_autosave,
        trigger_camera,
        trigger_cdaudio,
        trigger_changelevel,
        trigger_changetarget,
        trigger_counter,
        trigger_endsection,
        trigger_gravity,
        trigger_hurt,
        trigger_monsterjump,
        trigger_multiple,
        trigger_once,
        trigger_push,
        trigger_relay,
        trigger_teleport,
        trigger_transition,

        weapon_357,
        weapon_9mmAR,
        weapon_9mmhandgun,
        weapon_crossbow,
        weapon_crowbar,
        weapon_egon,
        weapon_gauss,
        weapon_handgrenade,
        weapon_hornetgun,
        weapon_rpg,
        weapon_satchel,
        weapon_shotgun,
        weapon_snark,
        weapon_tripmine,
        weaponbox,

        worldspawn,

        xen_hair,
        xen_plantlight,
        xen_spore_large,
        xen_spore_medium,
        xen_spore_small,
        xen_tree,

        CLASSNAME_TYPE_MAX
    };

    constexpr const char* classname_names[] = {
        "ambient_generic",

        "ammo_357",
        "ammo_9mmAR",
        "ammo_9mmbox",
        "ammo_9mmclip",
        "ammo_ARgrenades",
        "ammo_buckshot",
        "ammo_crossbow",
        "ammo_gaussclip",
        "ammo_rpgclip",

        "button_target",

        "cycler",
        "cycler_sprite",
        "cycler_wreckage",
        "cycler_weapon",

        "env_beam",
        "env_beverage",
        "env_blood",
        "env_bubbles",
        "env_explosion",
        "env_fade",
        "env_funnel",
        "env_glow",
        "env_global",
        "env_laser",
        "env_message",
        "env_rain",
        "env_render",
        "env_shake",
        "env_shooter",
        "env_smoker",
        "env_snow",
        "env_sound",
        "env_spark",
        "env_sprite",
        "env_fog",

        "func_breakable",
        "func_button",
        "func_conveyor",
        "func_door",
        "func_door_rotating",
        "func_friction",
        "func_guntarget",
        "func_healthcharger",
        "func_illusionary",
        "func_ladder",
        "func_monsterclip",
        "func_mortar_field",
        "func_pendulum",
        "func_plat",
        "func_platrot",
        "func_pushable",
        "func_recharge",
        "func_rot_button",
        "func_rotating",
        "func_tank",
        "func_tankcontrols",
        "func_tanklaser",
        "func_tankmortar",
        "func_tankrocket",
        "func_trackautochange",
        "func_trackchange",
        "func_tracktrain",
        "func_train",
        "func_traincontrols",
        "func_wall",
        "func_wall_toggle",
        "func_water",

        "game_counter",
        "game_counter_set",
        "game_end",
        "game_player_equip",
        "game_player_hurt",
        "game_player_team",
        "game_score",
        "game_team_master",
        "game_team_set",
        "game_text",
        "game_zone_player",

        "gibshooter",

        "info_bigmomma",
        "info_intermission",
        "info_landmark",
        "info_node",
        "info_node_air",
        "info_null",
        "info_player_coop",
        "info_player_deathmatch",
        "info_player_start",
        "info_target",
        "info_teleport_destination",
        "info_texlights",
        "infodecal",

        "item_airtank",
        "item_antidote",
        "item_battery",
        "item_healthkit",
        "item_longjump",
        "item_security",
        "item_suit",
        "world_items",

        "light",
        "light_environment",
        "light_spot",

        "momentary_door",
        "momentary_rot_button",

        "monster_alien_controller",
        "monster_alien_grunt",
        "monster_alien_slave",
        "monster_apache",
        "monster_barnacle",
        "monster_babycrab",
        "monster_barney",
        "monster_barney_dead",
        "monster_bigmomma",
        "monster_bullchicken",
        "monster_cockroach",
        "monster_flyer_flock",
        "monster_furniture",
        "monster_gargantua",
        "monster_generic",
        "monster_gman",
        "monster_grunt_repel",
        "monster_handgrenade",
        "monster_headcrab",
        "monster_hevsuit_dead",
        "monster_hgrunt_dead",
        "monster_houndeye",
        "monster_human_assassin",
        "monster_human_grunt",
        "monster_ichthyosaur",
        "monster_leech",
        "monster_miniturret",
        "monster_nihilanth",
        "monster_osprey",
        "monster_satchelcharge",
        "monster_scientist",
        "monster_scientist_dead",
        "monster_sentry",
        "monster_sitting_scientist",
        "monster_snark",
        "monster_tentacle",
        "monster_tripmine",
        "monster_turret",
        "monster_zombie",
        "monstermaker",

        "multi_manager",
        "multisource",

        "path_corner",
        "path_track",
        "player_loadsaved",
        "player_weaponstrip",

        "scripted_sentence",
        "scripted_sequence",
        "aiscripted_sequence",

        "speaker",

        "target_cdaudio",

        "trigger_auto",
        "trigger_autosave",
        "trigger_camera",
        "trigger_cdaudio",
        "trigger_changelevel",
        "trigger_changetarget",
        "trigger_counter",
        "trigger_endsection",
        "trigger_gravity",
        "trigger_hurt",
        "trigger_monsterjump",
        "trigger_multiple",
        "trigger_once",
        "trigger_push",
        "trigger_relay",
        "trigger_teleport",
        "trigger_transition",

        "weapon_357",
        "weapon_9mmAR",
        "weapon_9mmhandgun",
        "weapon_crossbow",
        "weapon_crowbar",
        "weapon_egon",
        "weapon_gauss",
        "weapon_handgrenade",
        "weapon_hornetgun",
        "weapon_rpg",
        "weapon_satchel",
        "weapon_shotgun",
        "weapon_snark",
        "weapon_tripmine",
        "weaponbox",

        "worldspawn",

        "xen_hair",
        "xen_plantlight",
        "xen_spore_large",
        "xen_spore_medium",
        "xen_spore_small",
        "xen_tree",
    };

    enum class parameter_type {
        classname,
        targetname,
        origin,
        light,
        pattern,
        style,
        fade,
        angle,
        map,
        landmark,
        model,
        message,
        skyname,
        chaptertitle,
        gametitle,
        newunit,
        wad,
        PARAMETER_TYPE_MAX
    };

    constexpr const char* parameter_names[] = {
        "classname",
        "targetname",
        "origin",
        "_light",
        "pattern",
        "style",
        "_fade",
        "angle",
        "map",
        "landmark",
        "model",
        "message",
        "skyname",
        "chaptertitle",
        "gametitle",
        "newunit",
        "wad",
    };

    struct entity_types {
        struct light {
            glm::ivec3 origin;
            glm::u8vec3 color;
            uint32_t intensity = 255;
            uint32_t fade = 1;
        };

        struct info_player_start {
            glm::ivec3 origin;
            float angle;
        };

        struct trigger_changelevel {
            std::string_view map;
            std::string_view landmark;
            std::string_view model;
        };

        struct info_landmark {
            std::string_view targetname;
            glm::ivec3 origin;
        };

        struct worldspawn {
            std::string_view message;
            std::string_view skyname;
            std::string_view chaptertitle;
            std::string_view wad;
            bool gametitle;
            bool newunit;
        };
    };

    using entity = std::variant<
            std::monostate,
            entity_types::light,
            entity_types::info_player_start,
            entity_types::trigger_changelevel,
            entity_types::info_landmark,
            entity_types::worldspawn>;

}

#endif //VOXLIFE_ENTITIES_H
