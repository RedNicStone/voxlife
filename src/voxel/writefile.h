
#ifndef VOXLIFE_VOXEL_WRITEFILE_H
#define VOXLIFE_VOXEL_WRITEFILE_H

#include <string_view>
#include <span>
#include <array>

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
    _COUNT_,
};

struct Voxel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    MaterialType type;
};

struct VoxelModel {
    std::span<Voxel> voxels;
    int pos_x;
    int pos_y;
    int pos_z;
    uint32_t size_x;
    uint32_t size_y;
    uint32_t size_z;
};

struct Model {
    std::string_view name;    // the name it should have when saved as a .vox file
    std::array<float, 3> pos; // relative to the scene
    std::array<float, 3> rot;
    std::array<int, 3> size;
    VoxelModel voxel_model;
};

void write_magicavoxel_model(std::string_view filename, std::span<const VoxelModel> in_models);

void write_teardown_level(std::string_view level_name, std::span<const Model> models);

#endif // VOXLIFE_VOXEL_WRITEFILE_H
