
#include <iostream>
#include <hl1/read_level.h>


int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <game path> <level name>" << std::endl;
        return 1;
    }

    std::string_view game_path = argv[1];
    std::string_view level_name = argv[2];

    return voxlife::hl1::load_level(game_path, level_name);
}
