
#include <bsp/readfile.h>

#include <iostream>


int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <bsp file>" << std::endl;
        return 1;
    }

    voxlife::bsp::bsp_handle handle;
    voxlife::bsp::open_file(argv[1], &handle);
    return 0;
}
