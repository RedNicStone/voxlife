
#ifndef VOXLIFE_READ_ENTITIES_H
#define VOXLIFE_READ_ENTITIES_H

#include <hl1/entities.h>
#include <bsp/read_file.h>

#include <string_view>
#include <vector>


namespace voxlife::hl1 {

    struct level_entities {
        std::vector<entity> entities[static_cast<size_t>(classname_type::CLASSNAME_TYPE_MAX)];
    };

    level_entities read_level(bsp::bsp_handle handle);
}


#endif //VOXLIFE_READ_ENTITIES_H
