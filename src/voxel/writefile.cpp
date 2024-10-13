
#define OGT_VOX_IMPLEMENTATION

#include <voxel/writefile.h>

#include <vector>
#include <cstdio>


void write_scene(std::string_view filename, std::span<Model> in_models, ogt_vox_palette const &palette) {
    ogt_vox_scene scene{};

    ogt_vox_group group{};
    group.name = "test";
    group.parent_group_index = k_invalid_group_index;
    group.layer_index = 0;
    group.hidden = false;
    group.transform = ogt_vox_transform_get_identity();

    ogt_vox_layer layer{};
    layer.name = "testlayer";
    layer.hidden = false;
    layer.color = {.r = 255, .g = 0, .b = 255, .a = 255};

    scene.groups = &group;
    scene.num_groups = 1;

    scene.layers = &layer;
    scene.num_layers = 1;

    std::vector<ogt_vox_instance> instances;
    std::vector<ogt_vox_model const *> models;

    models.resize(in_models.size());
    instances.reserve(in_models.size());

    for (int i = 0; i < in_models.size(); ++i) {
        in_models[i].model.voxel_data = in_models[i].voxels.data();
        models[i] = &in_models[i].model;

        auto instance = ogt_vox_instance{};
        instance.name = "testinst";
        instance.transform = ogt_vox_transform_get_identity();
        instance.transform.m30 = static_cast<float>(in_models[i].pos_x);
        instance.transform.m31 = static_cast<float>(in_models[i].pos_y);
        instance.transform.m32 = static_cast<float>(in_models[i].pos_z);
        instance.model_index = i;
        instance.layer_index = 0;
        instance.group_index = 0;
        instance.hidden = false;

        instances.emplace_back(instance);
    }

    scene.models = models.data();
    scene.num_models = models.size();
    scene.instances = instances.data();
    scene.num_instances = instances.size();

    scene.palette = palette;

    uint32_t buffer_size;
    auto *buffer_data = ogt_vox_write_scene(&scene, &buffer_size);

    auto *write_ptr = fopen(filename.data(), "wb");
    fwrite(buffer_data, buffer_size, 1, write_ptr);
    fclose(write_ptr);
    ogt_vox_free(buffer_data);
}
