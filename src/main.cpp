
#include <iostream>
#include <hl1/read_level.h>
#include <vector>


int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <game path> <level name>" << std::endl;
        return 1;
    }

    std::string_view game_path = argv[1];
    auto level_count = argc - 2;

    if (level_count == 1 && std::string_view(argv[2]) == "all")
        return voxlife::hl1::load_game_levels(game_path, {});

    std::vector<std::string_view> level_names;
    level_names.reserve(level_count);
    for (int i = 0; i < level_count; ++i)
        level_names.emplace_back(argv[2 + i]);

    return voxlife::hl1::load_game_levels(game_path, std::span(level_names));
}
