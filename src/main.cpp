
#include <bsp/readfile.h>
#include <wad/readfile.h>

#include <iostream>

extern void raster_test(voxlife::bsp::bsp_handle handle);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <bsp file>" << std::endl;
        return 1;
    }

    voxlife::wad::wad_handle wad_handle;
#if _WIN32
    voxlife::wad::open_file("C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/halflife.wad", &wad_handle);
#else
    voxlife::wad::open_file("/mnt/int/games/linux/steamapps/common/Half-Life/valve/halflife.wad", &wad_handle);
#endif

    voxlife::bsp::bsp_handle bsp_handle;
    voxlife::bsp::open_file(argv[1], wad_handle, &bsp_handle);

    raster_test(bsp_handle);

    return 0;
}
