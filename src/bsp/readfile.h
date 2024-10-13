
#include <string_view>

#ifndef VOXLIFE_READFILE_H
#define VOXLIFE_READFILE_H

namespace voxlife::bsp {

  typedef struct bsp_handle_T *bsp_handle;

  void open_file(std::string_view filename, bsp_handle* handle);

}

#endif //VOXLIFE_READFILE_H
