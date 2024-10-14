
#ifndef VOXLIFE_VOXEL_WRITEFILE_H
#define VOXLIFE_VOXEL_WRITEFILE_H

#include <string_view>
#include <span>

struct VoxelModel {
    std::span<uint32_t> voxels;
    int pos_x;
    int pos_y;
    int pos_z;
    uint32_t size_x;
    uint32_t size_y;
    uint32_t size_z;
};

void write_magicavoxel_model(std::string_view filename, std::span<VoxelModel> in_models);

#endif //VOXLIFE_VOXEL_WRITEFILE_H
