
#ifndef VOXLIFE_READ_LEVEL_H
#define VOXLIFE_READ_LEVEL_H

#include <string_view>


namespace voxlife::hl1 {

    int load_level(std::string_view game_path, std::string_view level_name);

}


#endif //VOXLIFE_READ_LEVEL_H
