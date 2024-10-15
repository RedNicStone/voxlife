
#ifndef VOXLIFE_BSP_READFILE_H
#define VOXLIFE_BSP_READFILE_H

#include <wad/readfile.h>

#include <string_view>
#include <glm/vec3.hpp>


namespace voxlife::bsp {



    typedef struct bsp_handle_T *bsp_handle;

    void open_file(std::string_view filename, wad::wad_handle resources, bsp_handle* handle);

}

#endif //VOXLIFE_BSP_READFILE_H
