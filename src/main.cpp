
#include <bsp/readfile.h>

#include <iostream>


int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <bsp file>" << std::endl;
        return 1;
    }

    voxelife::bsp::bsp_handle handle;
    voxelife::bsp::open_file(argv[1], &handle);
    return 0;
}
