
#ifndef VOXLIFE_VOXELIZE_POLYGON_H
#define VOXLIFE_VOXELIZE_POLYGON_H

#include <bsp/read_file.h>
#include <voxel/write_file.h>


namespace voxlife::voxel {

    bool voxelize_face(voxlife::bsp::bsp_handle handle, std::string_view level_name, voxlife::bsp::face& face, uint32_t face_index, Model& out_model);

}


#endif //VOXLIFE_VOXELIZE_POLYGON_H
