
#include <voxel/rasterize_polygon.h>
#include <voxel/writefile.h>
#include <bsp/readfile.h>

#include <cmath>
#include <iostream>
#include <string>
#include <format>

#include <glm/geometric.hpp>

using namespace voxlife::voxel;

std::pair<std::vector<glm::vec2>, std::vector<float>> project_face(voxlife::bsp::face &face) {
    constexpr float teardown_scale = 0.1f;  // Teardown uses a scale of 10 units per meter
    constexpr float hammer_scale = 0.0254f; // Hammer uses a scale of 1 unit is 1 inch

    constexpr float hammer_to_teardown_scale = hammer_scale / teardown_scale;

    std::vector<glm::vec2> points;
    std::vector<float> depths;
    points.reserve(face.vertices.size());
    depths.reserve(face.vertices.size());

    for (auto vertex : face.vertices) {
        vertex *= hammer_to_teardown_scale;

        glm::vec2 v;
        float depth;
        switch (face.facing) {
        case voxlife::bsp::face::PLANE_X:
        case voxlife::bsp::face::PLANE_ANYX:
            v.x = vertex.y;
            v.y = vertex.z;
            depth = vertex.x;
            break;
        case voxlife::bsp::face::PLANE_Y:
        case voxlife::bsp::face::PLANE_ANYY:
            v.x = vertex.z;
            v.y = vertex.x;
            depth = vertex.y;
            break;
        case voxlife::bsp::face::PLANE_Z:
        case voxlife::bsp::face::PLANE_ANYZ:
            v.x = vertex.x;
            v.y = vertex.y;
            depth = vertex.z;
            break;
        }

        points.push_back(v);
        depths.push_back(depth);
    }

    return {points, depths};
}

glm::vec3 unswizzle_vertex(voxlife::bsp::face::type facing, glm::vec3 xy_depth) {
    glm::vec2 v = {xy_depth.x, xy_depth.y};
    float depth = xy_depth.z;
    glm::vec3 vertex;
    switch (facing) {
    case voxlife::bsp::face::PLANE_X:
    case voxlife::bsp::face::PLANE_ANYX:
        vertex.y = v.x;
        vertex.z = v.y;
        vertex.x = depth;
        break;
    case voxlife::bsp::face::PLANE_Y:
    case voxlife::bsp::face::PLANE_ANYY:
        vertex.z = v.x;
        vertex.x = v.y;
        vertex.y = depth;
        break;
    case voxlife::bsp::face::PLANE_Z:
    case voxlife::bsp::face::PLANE_ANYZ:
        vertex.x = v.x;
        vertex.y = v.y;
        vertex.z = depth;
        break;
    }
    return {-vertex.x, vertex.z, vertex.y};
}

std::vector<glm::vec2> compute_face_uvs(voxlife::bsp::face &face) {
    std::vector<glm::vec2> uvs;
    uvs.reserve(face.vertices.size());

    for (auto vertex : face.vertices) {
        float u = glm::dot(face.texture_coords.s.axis, vertex) + face.texture_coords.s.shift;
        float v = glm::dot(face.texture_coords.t.axis, vertex) + face.texture_coords.t.shift;

        uvs.emplace_back(u, v);
    }

    return uvs;
}

template <typename T>
T bilinear_sample(glm::vec2 uv, glm::u32vec2 size, std::span<T> data) {
    uv = glm::fract(uv);
    glm::vec2 scaled_uv = uv * glm::vec2(size - glm::u32vec2(1, 1));
    glm::vec2 sub_texel = glm::fract(scaled_uv);
    glm::u32vec2 texel_min = glm::floor(scaled_uv);

    glm::u32vec2 texel_max = texel_min + glm::u32vec2(1, 1);
    if (texel_min.x > size.x - 1) [[unlikely]]
        texel_max.x = 0;
    else
        texel_max.x = texel_min.x + 1;

    if (texel_min.y > size.y - 1) [[unlikely]]
        texel_max.y = 0;
    else
        texel_max.y = texel_min.y + 1;

    T Q00 = data[texel_min.x + texel_min.y * size.x];
    T Q10 = data[texel_max.x + texel_min.y * size.x];
    T Q01 = data[texel_min.x + texel_max.y * size.x];
    T Q11 = data[texel_max.x + texel_max.y * size.x];

    return glm::mix(glm::mix(Q00, Q01, sub_texel.x), glm::mix(Q10, Q11, sub_texel.x), sub_texel.y);
}

bool voxelize_face(voxlife::bsp::bsp_handle handle, voxlife::bsp::face& face, uint32_t face_index, Model& out_model) {
    auto [points, depths] = project_face(face);
    auto uvs = compute_face_uvs(face);

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = -std::numeric_limits<float>::max();
    float max_y = -std::numeric_limits<float>::max();
    for (const auto& point : points) {
        min_x = std::min(min_x, point.x);
        min_y = std::min(min_y, point.y);
        max_x = std::max(max_x, point.x);
        max_y = std::max(max_y, point.y);
    }

    float min_z = std::numeric_limits<float>::max();
    float max_z = -std::numeric_limits<float>::max();
    for (const auto& depth : depths) {
        min_z = std::min(min_z, depth);
        max_z = std::max(max_z, depth);
    }

    auto depth = static_cast<uint32_t>(std::ceil(max_z - min_z) + 1.0f);

    grid_properties grid_info{};
    grid_info.width = static_cast<uint32_t>(std::ceil(max_x - min_x) + 1.0f);
    grid_info.height = static_cast<uint32_t>(std::ceil(max_y - min_y) + 1.0f);
    grid_info.origin = glm::vec2{min_x, min_y};

    std::cout << "Voxelizing face with " << face.vertices.size() << " vertices" << std::endl;
    std::cout << "Model dimensions: " << grid_info.width << " x " << grid_info.height << " x " << depth << std::endl;
    if (grid_info.width > 256 || grid_info.height > 256 || depth > 256) {
        std::cout << "Failed as object is too large for magicavoxel model" << std::endl;
        return false;
    }

    // if (grid_info.width == 1 || grid_info.height == 1 || depth == 1)
    //     return false;

    rasterizer_handle rasterizer;
    create_rasterizer(grid_info, &rasterizer);

    varying_handle v_depth;
    create_smooth_varying(rasterizer, std::span(depths), &v_depth);

    varying_handle v_uv;
    create_smooth_varying(rasterizer, std::span(uvs), &v_uv);

    auto depth_data = voxlife::voxel::get_smooth_varying_grid<float>(v_depth);
    std::fill(depth_data.begin(), depth_data.end(), -std::numeric_limits<float>::max());

    rasterize_polygon(rasterizer, std::span(points));

    VoxelModel model{};
    model.size.x = grid_info.width;
    model.size.y = grid_info.height;
    model.size.z = depth;
    model.pos = glm::vec3(0, 0, 0);

    std::vector<Voxel> voxels{};
    voxels.resize(model.size.x * model.size.y * model.size.z);

    auto uv_data = voxlife::voxel::get_smooth_varying_grid<glm::vec2>(v_uv);
    auto texture = voxlife::bsp::get_texture_data(handle, face.texture_id);

    for (auto x = 0; x < grid_info.width; ++x) {
        for (auto y = 0; y < grid_info.height; ++y) {
            float depth_value = depth_data[y * grid_info.width + x] - min_z;

            if (depth_value < 0 || depth_value > static_cast<float>(depth))
                continue;

            glm::vec2 uv_value = uv_data[y * grid_info.width + x];
            auto color = bilinear_sample<glm::u8vec3>(uv_value, texture.size, texture.data);

            auto voxel_depth = static_cast<uint32_t>(std::round(depth_value));
            auto& voxel = voxels[voxel_depth * model.size.x * model.size.y + y * model.size.x + x];
            voxel.material = WOOD;
            voxel.color = color;
        }
    }

    model.voxels = std::span(voxels);
    out_model.name = std::format("{}", face_index);
    const float CM_TO_METERS = 0.1;
    out_model.pos = unswizzle_vertex(face.facing, glm::vec3(min_x, min_y, min_z)) * CM_TO_METERS;
    out_model.size = {model.size.x, model.size.z, model.size.y};
    out_model.rot = {0, 0, 0};

    switch (face.facing) {
    case voxlife::bsp::face::PLANE_X:
    case voxlife::bsp::face::PLANE_ANYX:
        // out_model.rot = {-90, 0, 0};
        break;
    case voxlife::bsp::face::PLANE_Y:
    case voxlife::bsp::face::PLANE_ANYY:
        // out_model.rot = {0, 90, 90};
        break;
    case voxlife::bsp::face::PLANE_Z:
    case voxlife::bsp::face::PLANE_ANYZ:
        // out_model.rot = {0, -90, 0};
        break;
    }

    write_magicavoxel_model(std::format("brush/{}.vox", face_index), std::span(&model, 1));
    return true;
}

void raster_test(voxlife::bsp::bsp_handle handle) {
    auto faces = voxlife::bsp::get_model_faces(handle, 0);
    std::vector<Model> models;
    models.reserve(faces.size());

    uint32_t count = 0;
    for (auto& face : faces) {
        try {
            models.push_back({});
            voxelize_face(handle, face, count++, models.back());
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    std::vector<Light> lights;
    write_teardown_level("test", models, lights);
}
