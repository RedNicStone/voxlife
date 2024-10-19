
#ifndef VOXLIFE_VOXEL_WRITEFILE_H
#define VOXLIFE_VOXEL_WRITEFILE_H

#include <string_view>
#include <span>
#include <array>
#include <glm/vec3.hpp>
#include <string>

enum MaterialType : uint8_t {
    AIR,
    UN_PHYSICAL,
    HARD_MASONRY,
    HARD_METAL,
    PLASTIC,
    HEAVY_METAL,
    WEAK_METAL,
    PLASTER,
    BRICK,
    CONCRETE,
    WOOD,
    ROCK,
    DIRT,
    GRASS,
    GLASS,
    MATERIAL_ALL_TYPES,
    MATERIAL_TYPE_MAX,
};

struct Voxel {
    glm::u8vec3 color;
    MaterialType material = MaterialType::AIR;
};

struct VoxelModel {
    std::span<const Voxel> voxels;
    glm::i32vec3 pos;
    glm::u32vec3 size;
};

struct Model {
    std::string name; // the name it should have when saved as a .vox file
    glm::vec3 pos;    // relative to the scene
    glm::vec3 rot;
    glm::u32vec3 size;
};

struct Light {
    glm::vec3 pos;
    glm::u8vec3 color;
    float intensity;
};

struct ExtraInfo {
    glm::vec3 spawn_pos;
    glm::vec3 spawn_rot;
};

void write_magicavoxel_model(std::string_view filename, std::span<const VoxelModel> in_models);

void write_teardown_level(std::string_view level_name, std::span<const Model> models, std::span<const Light> lights, const ExtraInfo &info);

#endif // VOXLIFE_VOXEL_WRITEFILE_H
