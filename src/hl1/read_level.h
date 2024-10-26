
#ifndef VOXLIFE_READ_LEVEL_H
#define VOXLIFE_READ_LEVEL_H

#include <string_view>
#include <span>


namespace voxlife::hl1 {

    int load_game_levels(std::string_view game_path, std::span<const std::string_view> level_names);

}


#endif //VOXLIFE_READ_LEVEL_H
