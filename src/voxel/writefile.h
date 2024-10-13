
#ifndef VOXLIFE_VOXEL_WRITEFILE_H
#define VOXLIFE_VOXEL_WRITEFILE_H

#include <ogt_vox.h>
#include <string_view>
#include <span>


struct Model {
    ogt_vox_model model;
    std::span<uint8_t> voxels; // 0-256, where 0 is the empty index
    int pos_x;
    int pos_y;
    int pos_z;
};

void write_scene(std::string_view filename, std::span<Model> in_models, ogt_vox_palette const &palette);

#endif //VOXLIFE_VOXEL_WRITEFILE_H
