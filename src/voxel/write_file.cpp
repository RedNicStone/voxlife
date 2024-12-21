#include <numeric>
#define OGT_VOX_IMPLEMENTATION
#include <ogt_vox.h>

#include <voxel/write_file.h>

#include <vector>
#include <cstdio>
#include <limits>
#include <format>
#include <span>
#include <array>
#include <algorithm>
#include <random>
#include <glm/geometric.hpp>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <set>
#include <fstream>
#include <unordered_set>

auto rgb_to_oklab(glm::vec3 rgb) -> glm::vec3 {
    // Normalize the RGB values to the range [0, 1]
#pragma unroll 3
    for (int i = 0; i < 3; ++i)
        rgb[i] /= 255.0f;
    // Convert to the XYZ color space
    glm::vec3 xyz;
    xyz[0] = 0.4124564f * rgb[0] + 0.3575761f * rgb[1] + 0.1804375f * rgb[2];
    xyz[1] = 0.2126729f * rgb[0] + 0.7151522f * rgb[1] + 0.0721750f * rgb[2];
    xyz[2] = 0.0193339f * rgb[0] + 0.1191920f * rgb[1] + 0.9503041f * rgb[2];
    // Normalize XYZ
    float x = xyz[0] / 0.95047f; // D65 white point
    float y = xyz[1] / 1.0f;
    float z = xyz[2] / 1.08883f;
    // Convert to Oklab
    float l = 0.210454f * x + 0.793617f * y - 0.004072f * z;
    float a = 1.977665f * x - 0.510530f * y - 0.447580f * z;
    float b = 0.025334f * x + 0.338572f * y - 0.602190f * z;
    return {l, a, b};
}

auto oklab_to_rgb(glm::vec3 oklab) -> glm::vec3 {
    // Convert to XYZ
    glm::vec3 xyz;
    xyz[0] = +0.44562442079f * oklab[0] + 0.46266924383f * oklab[1] - 0.34689397498f * oklab[2];
    xyz[1] = +1.14528157354f * oklab[0] - 0.12294697715f * oklab[1] + 0.08363642948f * oklab[2];
    xyz[2] = +0.66266414585f * oklab[0] - 0.04966064087f * oklab[1] - 1.62817592248f * oklab[2];
    // Un-normalize XYZ
    float x = xyz[0] * 0.95047f; // D65 white point
    float y = xyz[1] * 1.0f;
    float z = xyz[2] * 1.08883f;
    // Convert to the RGB color space
    glm::vec3 rgb;
    rgb[0] = 3.2404542f * x - 1.5371385f * y - 0.4985314f * z;
    rgb[1] = -0.9692660f * x + 1.8760108f * y + 0.0415560f * z;
    rgb[2] = 0.0556434f * x - 0.2040259f * y + 1.0572252f * z;
    // Bring back into range [0, 255]
#pragma unroll 3
    for (int i = 0; i < 3; ++i)
        rgb[i] *= 255.0f;
    return rgb;
}

struct MaterialTypeSlot {
    uint32_t slot_count;
    uint32_t slot_offset;
};

constexpr std::array<MaterialTypeSlot, MATERIAL_TYPE_MAX> material_type_slots{{
    {0, 0},    // AIR
    {16, 224}, // UN_PHYSICAL
    {8, 176},  // HARD_MASONRY
    {8, 168},  // HARD_METAL
    {16, 152}, // PLASTIC
    {16, 136}, // HEAVY_METAL
    {16, 120}, // WEAK_METAL
    {16, 104}, // PLASTER
    {16, 88},  // BRICK
    {16, 72},  // CONCRETE
    {16, 56},  // WOOD
    {16, 40},  // ROCK
    {16, 24},  // DIRT
    {16, 8},   // GRASS
    {8, 0},    // GLASS
    {253, 1},  // MATERIAL_ALL_TYPES
}};

struct MaterialData {
    std::unordered_map<uint32_t, size_t> color_to_index;  // Packed RGB color to unique color index
    std::vector<glm::u8vec3> unique_colors;               // Unique RGB colors
    std::vector<std::pair<size_t, size_t>> voxel_indices; // Pairs of (model index, voxel index)
    std::vector<size_t> voxel_color_indices;              // Unique color index per voxel
    std::vector<glm::vec3> unique_oklab_colors;           // Oklab colors
    std::vector<int> cluster_assignments;                 // Cluster index per unique color
    std::vector<glm::vec3> cluster_centers;               // Centroids in Oklab space
    std::vector<glm::u8vec3> palette_entries;             // Final palette entries in RGB
};

inline uint32_t pack_rgb(const glm::u8vec3 &color) {
    return (static_cast<uint32_t>(color.r) << 16) |
           (static_cast<uint32_t>(color.g) << 8) |
           static_cast<uint32_t>(color.b);
}

void kmeans(const std::vector<glm::vec3> &data_points, size_t k,
            std::vector<int> &assignments, std::vector<glm::vec3> &centroids,
            int max_iterations = 100) {
    size_t n = data_points.size();
    assignments.resize(n);
    centroids.resize(k);

    if (n <= static_cast<size_t>(k)) {
        for (size_t i = 0; i < n; ++i) {
            assignments[i] = static_cast<int>(i);
            centroids[i] = data_points[i];
        }
        return;
    }

    std::vector<size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 rng(std::random_device{}());
    std::shuffle(indices.begin(), indices.end(), rng);

    for (int i = 0; i < k; ++i)
        centroids[i] = data_points[indices[i]];

    std::vector<glm::vec3> new_centroids(k);
    std::vector<int> counts(k);
    bool changed = true;
    int iterations = 0;

    while (changed && iterations < max_iterations) {
        changed = false;
        ++iterations;

#pragma omp parallel for schedule(static)
        for (int i = 0; i < n; ++i) {
            const glm::vec3 &point = data_points[i];
            float min_distance = std::numeric_limits<float>::max();
            int best_cluster = -1;
            for (int j = 0; j < k; ++j) {
                float distance = glm::dot(point - centroids[j], point - centroids[j]);
                if (distance < min_distance) {
                    min_distance = distance;
                    best_cluster = j;
                }
            }
            if (assignments[i] != best_cluster) {
                assignments[i] = best_cluster;
                changed = true;
            }
        }

        std::fill(new_centroids.begin(), new_centroids.end(), glm::vec3(0.0f));
        std::fill(counts.begin(), counts.end(), 0);

#pragma omp parallel for schedule(static)
        for (int i = 0; i < n; ++i) {
            int cluster = assignments[i];
#pragma omp atomic
            counts[cluster] += 1;
#pragma omp critical
            new_centroids[cluster] += data_points[i];
        }

        for (int j = 0; j < k; ++j) {
            if (counts[j] > 0)
                centroids[j] = new_centroids[j] / static_cast<float>(counts[j]);
            else
                centroids[j] = data_points[rng() % n];
        }
    }
}

auto generate_palette(std::span<const VoxelModel> models) -> std::pair<ogt_vox_palette, std::vector<std::vector<uint8_t>>> {
    std::array<MaterialData, MaterialType::MATERIAL_TYPE_MAX> materials_data;

    for (size_t model_idx = 0; model_idx < models.size(); ++model_idx) {
        const VoxelModel &model = models[model_idx];
        for (size_t voxel_idx = 0; voxel_idx < model.voxels.size(); ++voxel_idx) {
            const Voxel &voxel = model.voxels[voxel_idx];
            MaterialType material = voxel.material;
            if (material == MaterialType::AIR)
                continue;

            auto &mat_data = materials_data[material];

            mat_data.voxel_indices.emplace_back(model_idx, voxel_idx);

            const uint32_t packed_color = pack_rgb(voxel.color);

            auto it = mat_data.color_to_index.find(packed_color);
            size_t color_index;
            if (it == mat_data.color_to_index.end()) {
                color_index = mat_data.unique_colors.size();
                mat_data.unique_colors.push_back(voxel.color);
                mat_data.color_to_index[packed_color] = color_index;
            } else
                color_index = it->second;

            mat_data.voxel_color_indices.push_back(color_index);
        }
    }

    std::pair<ogt_vox_palette, std::vector<std::vector<uint8_t>>> model_data;
    auto &color_palette = model_data.first;
    auto &model_indices = model_data.second;

    model_indices.resize(models.size());
    for (size_t i = 0; i < models.size(); ++i)
        model_indices[i].resize(models[i].voxels.size());

#pragma omp parallel for schedule(dynamic)
    for (int material = 0; material < MaterialType::MATERIAL_TYPE_MAX; ++material) {
        auto &mat_data = materials_data[material];
        const auto &mat_info = material_type_slots[material];

        if (mat_data.unique_colors.empty() || mat_info.slot_count == 0)
            continue;

        const size_t num_unique_colors = mat_data.unique_colors.size();

        mat_data.unique_oklab_colors.resize(num_unique_colors);
        for (size_t i = 0; i < num_unique_colors; ++i) {
            glm::vec3 rgb = glm::vec3(mat_data.unique_colors[i]) / 255.0f;
            mat_data.unique_oklab_colors[i] = rgb_to_oklab(rgb);
        }

        int k = static_cast<int>(mat_info.slot_count);
        kmeans(mat_data.unique_oklab_colors, k, mat_data.cluster_assignments, mat_data.cluster_centers);

        mat_data.palette_entries.resize(k);
        for (int i = 0; i < k; ++i) {
            glm::vec3 rgb = oklab_to_rgb(mat_data.cluster_centers[i]);
            rgb = glm::clamp(rgb * 255.0f, 0.0f, 255.0f);
            mat_data.palette_entries[i] = glm::u8vec3(rgb);
        }

        for (size_t i = 0; i < mat_data.voxel_indices.size(); ++i) {
            const auto &voxel_idx_pair = mat_data.voxel_indices[i];
            const size_t model_idx = voxel_idx_pair.first;
            const size_t voxel_idx = voxel_idx_pair.second;
            const size_t color_idx = mat_data.voxel_color_indices[i];
            const int cluster_idx = mat_data.cluster_assignments[color_idx];

            uint32_t palette_index = mat_info.slot_offset + cluster_idx;
            model_indices[model_idx][voxel_idx] = palette_index;
        }

        for (int i = 0; i < k; ++i) {
            color_palette.color[mat_info.slot_offset + i].r = mat_data.palette_entries[i].r;
            color_palette.color[mat_info.slot_offset + i].g = mat_data.palette_entries[i].g;
            color_palette.color[mat_info.slot_offset + i].b = mat_data.palette_entries[i].b;
            color_palette.color[mat_info.slot_offset + i].a = 255;
        }
    }

    return model_data;
}

void write_magicavoxel_model(std::string_view filename, std::span<const VoxelModel> in_models) {
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

    auto [palette, voxels] = generate_palette(in_models);

    models.resize(in_models.size());
    instances.reserve(in_models.size());

    for (int i = 0; i < in_models.size(); ++i) {
        auto *model = new ogt_vox_model{};
        model->size_x = in_models[i].size.x;
        model->size_y = in_models[i].size.y;
        model->size_z = in_models[i].size.z;
        model->voxel_data = voxels[i].data();
        models[i] = model;

        auto instance = ogt_vox_instance{};
        instance.name = "testinst";
        instance.transform = ogt_vox_transform_get_identity();
        instance.transform.m30 = static_cast<float>(in_models[i].pos.x);
        instance.transform.m31 = static_cast<float>(in_models[i].pos.y);
        instance.transform.m32 = static_cast<float>(in_models[i].pos.z);
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
    if (write_ptr) {
        fwrite(buffer_data, buffer_size, 1, write_ptr);
        fclose(write_ptr);
    } else {
        std::cout << "Failed to open " << filename << " for writing. skipping..." << std::endl;
    }
    ogt_vox_free(buffer_data);

    for (auto const *model : models)
        delete model;
}

void write_teardown_level(const LevelInfo &info) {
    auto xml_str = std::string{};

    auto level_rot = std::array<float, 3>{0, 0, 0};

    xml_str += "<prefab version=\"1.6.0\">\n";
    xml_str += std::format(
        "<group name=\"instance=MOD/levels/{}.xml\">\n<group tags=\"{}\" pos=\"{:.3f} {:.3f} {:.3f}\" rot=\"{:.3f} {:.3f} {:.3f}\">\n",
        info.name,
        info.name,
        info.level_pos.x, info.level_pos.y, info.level_pos.z,
        level_rot[0], level_rot[1], level_rot[2]);

    xml_str += std::format(
        R"(<spawnpoint tags="{}" pos="{:.3f} {:.3f} {:.3f}" rot="{:.3f} {:.3f} {:.3f}"/>)"
        "\n",
        info.name,
        info.spawn_pos.x, info.spawn_pos.y, info.spawn_pos.z,
        info.spawn_rot.x, info.spawn_rot.y, info.spawn_rot.z);

    xml_str += std::format(
        R"(<location tags="playerspawn {}" pos="{:.3f} {:.3f} {:.3f}" rot="{:.3f} {:.3f} {:.3f}"/>)"
        "\n",
        info.name,
        info.spawn_pos.x, info.spawn_pos.y, info.spawn_pos.z,
        info.spawn_rot.x, -info.spawn_rot.y, info.spawn_rot.z);

    xml_str += std::format(
        R"(<location tags="env {} tag_skybox=MOD/{}.dds tag_skyboxbrightness={} tag_sunColorTintR={:.3f} tag_sunColorTintG={:.3f} tag_sunColorTintB={:.3f} tag_sunDirX={:.3f} tag_sunDirY={:.3f} tag_sunDirZ={:.3f}"/>)"
        "\n",
        info.name,
        info.environment.skybox,
        info.environment.brightness,
        info.environment.sun_color.r, info.environment.sun_color.g, info.environment.sun_color.b,
        info.environment.sun_dir.x, info.environment.sun_dir.y, info.environment.sun_dir.z);

    xml_str += std::format(
        R"(<environment tags="{}" skybox="MOD/{}.dds" skyboxbrightness="{}" skyboxrot="-90" constant="0.003 0.003 0.003" ambient="1" fogParams="0 0 0 0" sunColorTint="{:.3f} {:.3f} {:.3f}" sunDir="{:.3f} {:.3f} {:.3f}" sunSpread="0"/>)"
        "\n",
        info.name,
        info.environment.skybox,
        info.environment.brightness,
        info.environment.sun_color.r, info.environment.sun_color.g, info.environment.sun_color.b,
        info.environment.sun_dir.x, info.environment.sun_dir.y, info.environment.sun_dir.z);
    for (auto const &location : info.locations) {
        xml_str += std::format(
            R"(<location tags="{} targetname_{}" name="{}" pos="{:.3f} {:.3f} {:.3f}"/>)"
            "\n",
            info.name,
            location.name,
            location.name,
            location.pos.x, location.pos.y, location.pos.z);
    }
    for (auto const &npc : info.npcs) {
        xml_str += std::format(
            R"(<script tags="{}" pos="0.0 0.0 0.0" file="MOD/characters/idle-anim.lua">)"
            "\n"
            R"(<instance tags="{}" pos="{:.3f} {:.3f} {:.3f}" rot="{:.3f} {:.3f} {:.3f}" file="MOD/characters/{}.xml"/>)"
            "\n"
            R"(</script>)"
            "\n",
            info.name,
            info.name,
            npc.pos.x, npc.pos.y + 0.15f, npc.pos.z,
            npc.rot.x, npc.rot.y, npc.rot.z,
            npc.path_name);
    }
    for (auto const &trigger : info.triggers) {
        xml_str += std::format(
            R"(<trigger tags="{} changelevel map={} landmark={}" name="{}" pos="{:.3f} {:.3f} {:.3f}" type="box" size="{:.3f} {:.3f} {:.3f}"/>)"
            "\n",
            info.name,
            trigger.map,
            trigger.landmark,
            trigger.map,
            trigger.pos.x + trigger.size.x * 0.5f, trigger.pos.y, trigger.pos.z + trigger.size.z * 0.5f,
            trigger.size.x, trigger.size.y, trigger.size.z);
    }

    for (auto const &light : info.lights) {
        xml_str += std::format(
            R"(<light tags="{}" pos="{:.3f} {:.3f} {:.3f}" color="{} {} {}" scale="{}"/>)"
            "\n",
            info.name,
            light.pos.x, light.pos.y, light.pos.z,
            float(light.color.r) / 255.0f,
            float(light.color.g) / 255.0f,
            float(light.color.b) / 255.0f,
            float(light.intensity) * 0.1f);
    }

    for (auto const &model : info.models) {
        auto model_filepath = std::format("MOD/brush/{}/{}.vox", info.name, model.name);

        xml_str += std::format(
            R"(<voxbox name="{}" tags="{}" pos="{:.3f} {:.3f} {:.3f}" rot="{:.3f} {:.3f} {:.3f}" size="{} {} {}" brush="{}"/>)"
            "\n",
            model_filepath,
            info.name,
            model.pos.x, model.pos.y, model.pos.z,
            model.rot.x, model.rot.y, model.rot.z,
            model.size.x, model.size.y, model.size.z,
            model_filepath);
    }

    xml_str += "</group>\n</group>\n</prefab>\n";

    std::filesystem::create_directories("levels");

    auto level_filepath = std::format("levels/{}.xml", info.name);
    auto *write_ptr = fopen(level_filepath.data(), "wb");
    if (write_ptr) {
        fwrite(xml_str.data(), xml_str.size(), 1, write_ptr);
        fclose(write_ptr);
    }
}
