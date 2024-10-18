
#ifndef VOXLIFE_WAD_READFILE_H
#define VOXLIFE_WAD_READFILE_H

#include <string_view>

namespace voxlife::wad {

    typedef struct wad_handle_T *wad_handle;

    void open_file(std::string_view filename, wad_handle* handle);

    const void* get_entry(wad_handle handle, std::string_view name);
    size_t get_entry_size(wad_handle handle, std::string_view name);

}

#endif //VOXLIFE_WAD_READFILE_H
