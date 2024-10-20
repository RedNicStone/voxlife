
#ifndef VOXLIFE_COORIDNATES_H
#define VOXLIFE_COORIDNATES_H


namespace voxlife::voxel {

    constexpr float teardown_scale = 0.1f;  // Teardown uses a scale of 10 units per meter
    constexpr float hammer_scale = 0.0254f; // Hammer uses a scale of 1 unit is 1 inch

    constexpr float hammer_to_teardown_scale = hammer_scale / teardown_scale;
    constexpr float teardown_to_hammer_scale = teardown_scale / hammer_scale;
    constexpr float decimeter_to_meter = 0.1;

}


#endif //VOXLIFE_COORIDNATES_H
