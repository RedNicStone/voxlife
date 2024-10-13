
#include <bsp/readfile.h>
#include <wad/readfile.h>

#include <iostream>


int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <bsp file>" << std::endl;
        return 1;
    }

    voxlife::wad::wad_handle wad_handle;
    voxlife::wad::open_file("/mnt/int/games/linux/steamapps/common/Half-Life/valve/halflife.wad", &wad_handle);

    voxlife::bsp::bsp_handle bsp_handle;
    voxlife::bsp::open_file(argv[1], wad_handle, &bsp_handle);
    return 0;
}
