
#include <string_view>

#ifndef VOXELIFE_READFILE_H
#define VOXELIFE_READFILE_H

namespace voxelife::bsp {

  typedef struct bsp_handle_T *bsp_handle;

  void open_file(std::string_view filename, bsp_handle* handle);

}

#endif //VOXELIFE_READFILE_H
