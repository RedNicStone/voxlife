#include <voxel/voxelize_polygon.h>

#include <voxel/rasterize_polygon.h>
#include <voxel/cooridnates.h>

#include <cmath>
#include <iostream>
#include <format>
#include <algorithm>
#include <filesystem>

#include <glm/geometric.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vec_swizzle.hpp>


namespace voxlife::voxel {


    std::vector<glm::vec3> convert_coordinates(std::span<glm::vec3> points) {
        /*
         * The BSP is in Hammer coordinates, but the voxels are in Teardown coordinates.
         * This function converts the BSP coordinates to Teardown coordinates.
         *
         * GoldSrc coordinate system:
         * Z-up right-handed
         * - X: Forward
         * - Y: Left
         * - Z: Up
         *
         * Teardown coordinate system:
         * Y-up left-handed
         * - X: Forward
         * - Y: Up
         * - Z: Right
         *
         * So the conversion from Hammer to Teardown is:
         * X (Forward) -> +X (Forward)
         * Y (Left)    -> -Z (Right)
         * Z (Up)      -> +Y (Up)
         *
         * Additionally, the BSP is in Hammer units, but the voxels are in Teardown units.
         * Hammer units are the same as GoldSrc units, which are about 1 inch in length.
         * Teardown units are exactly 1/10th of a meter or one decimeter.
         */

        std::vector<glm::vec3> converted_points;
        converted_points.reserve(points.size());

        std::transform(points.begin(), points.end(), std::back_inserter(converted_points), [](glm::vec3 point) {
            point *= hammer_to_teardown_scale;
            point = glm::xzy(point);
            point.z *= -1.0f;
            // todo: This currently breaks texture mapping, probably need to flip this in the ST vector as well

            return point;
        });

        return converted_points;
    }

    std::pair<std::vector<glm::vec2>, std::vector<float>> project_face(std::span<glm::vec3> points, voxlife::bsp::face &face) {
        std::vector<glm::vec2> polygon;
        std::vector<float> depths;
        polygon.reserve(face.vertices.size());
        depths.reserve(face.vertices.size());

        for (auto vertex : points) {
            // Un-swizzle so that it aligns with the BSP coordinate system again :abyss:
            vertex = glm::xzy(vertex);

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

            polygon.push_back(v);
            depths.push_back(depth);
        }

        return { polygon, depths };
    }

    std::vector<glm::vec2> compute_face_uvs(voxlife::bsp::face &face) {
        std::vector<glm::vec2> uvs;
        uvs.reserve(face.vertices.size());

        for (auto vertex : face.vertices) {
            glm::vec2 uv;
            uv.s = glm::dot(face.texture_coords.s.axis, vertex) + face.texture_coords.s.shift;
            uv.t = glm::dot(face.texture_coords.t.axis, vertex) + face.texture_coords.t.shift;

            uvs.push_back(uv);
        }

        return uvs;
    }

    template <typename T>
    T bilinear_sample(glm::vec2 uv, glm::u32vec2 size, std::span<T> data) {
        /*
         * Sample a bilinearly interpolated value from a 2D array of values.
         * The array is assumed to be in row-major order.
         *
         * The UV coordinates are tiled over the range [0, size] before sampling.
         */

        glm::vec2 scaled_uv = glm::mod(uv, glm::vec2(size));
        glm::vec2 sub_texel = glm::fract(scaled_uv);
        glm::u32vec2 texel_min = glm::floor(scaled_uv);

        if (texel_min.x > size.x - 1) [[unlikely]]
            texel_min.x = 0;

        if (texel_min.y > size.y - 1) [[unlikely]]
            texel_min.y = 0;

        glm::u32vec2 texel_max = texel_min + glm::u32vec2(1, 1);
        if (texel_max.x > size.x - 1) [[unlikely]]
            texel_max.x = 0;

        if (texel_max.y > size.y - 1) [[unlikely]]
            texel_max.y = 0;


        T Q00 = data[texel_min.x + texel_min.y * size.x];
        T Q10 = data[texel_max.x + texel_min.y * size.x];
        T Q01 = data[texel_min.x + texel_max.y * size.x];
        T Q11 = data[texel_max.x + texel_max.y * size.x];

        return glm::mix(glm::mix(Q00, Q01, sub_texel.x), glm::mix(Q10, Q11, sub_texel.x), sub_texel.y);
    }

    bool voxelize_face(voxlife::bsp::bsp_handle handle, std::string_view level_name, voxlife::bsp::face& face, uint32_t face_index, Model& out_model) {
        auto points = convert_coordinates(face.vertices);
        auto [polygon, depths] = project_face(points, face);
        auto uvs = compute_face_uvs(face);

        // Determine the signed area of the polygon
        float signed_area = 0.0f;
        for (auto it = points.begin(); it != points.end() - 1; ++it) {
            auto first_point = *it;
            auto second_point = *(it + 1);

            signed_area += first_point.x * second_point.y - second_point.x * first_point.y;
        }
        signed_area += points.back().x * points.front().y - points.front().x * points.back().y;
        bool is_clockwise = signed_area < 0.0f;

        auto projected_min = glm::vec3(+std::numeric_limits<float>::max());
        auto projected_max = glm::vec3(-std::numeric_limits<float>::max());

        for (const auto& point : polygon) {
            projected_min.x = std::min(projected_min.x, point.x);
            projected_min.y = std::min(projected_min.y, point.y);
            projected_max.x = std::max(projected_max.x, point.x);
            projected_max.y = std::max(projected_max.y, point.y);
        }

        for (const auto& depth : depths) {
            projected_min.z = std::min(projected_min.z, depth);
            projected_max.z = std::max(projected_max.z, depth);
        }

        auto world_min = glm::vec3(+std::numeric_limits<float>::max());
        auto world_max = glm::vec3(-std::numeric_limits<float>::max());

        // todo: Dont compute this twice, compute once then project
        for (const auto& point : points) {
            world_min = glm::min(world_min, point);
            world_max = glm::max(world_max, point);
        }

        auto depth = static_cast<uint32_t>(std::ceil(projected_max.z) - std::floor(projected_min.z) + 1.0f);

        grid_properties grid_info{};
        grid_info.width = static_cast<uint32_t>(std::ceil(projected_max.x) - std::floor(projected_min.x));
        grid_info.height = static_cast<uint32_t>(std::ceil(projected_max.y) - std::floor(projected_min.y));
        grid_info.origin = glm::vec2{ std::floor(projected_min.x), std::floor(projected_min.y) };

        // std::cout << "Voxelizing face with " << face.vertices.size() << " vertices" << std::endl;
        // std::cout << "Model dimensions: " << grid_info.width << " x " << grid_info.height << " x " << depth << std::endl;
        if (grid_info.width > 256 || grid_info.height > 256 || depth > 256) {
            std::cout << "Failed as object is too large for magicavoxel model" << std::endl;
            return false;
        }
        if (grid_info.width == 0 || grid_info.height == 0 || depth == 0) {
            std::cout << "Failed as object is too small for magicavoxel model" << std::endl;
            return false;
        }

        rasterizer_handle rasterizer;
        create_rasterizer(grid_info, &rasterizer);

        varying_handle v_depth;
        create_smooth_varying(rasterizer, std::span(depths), &v_depth);

        varying_handle v_uv;
        create_smooth_varying(rasterizer, std::span(uvs), &v_uv);

        auto depth_data = voxlife::voxel::get_smooth_varying_grid<float>(v_depth);
        std::fill(depth_data.begin(), depth_data.end(), -std::numeric_limits<float>::max());

        rasterize_polygon_conservatively(rasterizer, std::span(polygon));

        auto max_size = glm::u32vec3();
        auto min_size = glm::u32vec3(std::numeric_limits<uint32_t>::max());
        for (uint32_t x = 0; x < grid_info.width; ++x) {
            for (uint32_t y = 0; y < grid_info.height; ++y) {
                float depth_value = depth_data[y * grid_info.width + x] - projected_min.z;
                if (depth_value < 0 || depth_value > static_cast<float>(depth))
                    continue;

                max_size.x = std::max(max_size.x, x + 1);
                max_size.y = std::max(max_size.y, y + 1);
                max_size.z = std::max(max_size.z, static_cast<uint32_t>(std::ceil(depth_value)) + 1);
                min_size.x = std::min(min_size.x, x);
                min_size.y = std::min(min_size.y, y);
                min_size.z = std::min(min_size.z, static_cast<uint32_t>(std::floor(depth_value)));
            }
        }

        if (min_size.x == std::numeric_limits<uint32_t>::max() ||
            min_size.y == std::numeric_limits<uint32_t>::max() ||
            min_size.z == std::numeric_limits<uint32_t>::max()) {
            std::cout << "Failed as object has generated no valid voxels" << std::endl;
            return false;
        }

        VoxelModel model{};
        model.size = max_size - min_size;
        model.pos = glm::vec3(0, 0, 0);

        std::vector<Voxel> voxels{};
        voxels.resize(model.size.x * model.size.y * model.size.z);

        auto uv_data = voxlife::voxel::get_smooth_varying_grid<glm::vec2>(v_uv);
        auto texture = voxlife::bsp::get_texture_data(handle, face.texture_id);

        for (uint32_t x = 0; x < model.size.x; ++x) {
            for (uint32_t y = 0; y < model.size.y; ++y) {
                float depth_value = depth_data[(y + min_size.y) * grid_info.width + x + min_size.x] - projected_min.z;
                if (depth_value < 0 || depth_value > static_cast<float>(depth))
                    continue;

                glm::vec2 uv_value = uv_data[(y + min_size.y) * grid_info.width + x + min_size.x];
                auto color = bilinear_sample<glm::u8vec3>(uv_value, texture.size, texture.data);

                auto bottom_voxel_depth = static_cast<uint32_t>(std::floor(depth_value)) - min_size.z;
                auto& bottom_voxel = voxels[bottom_voxel_depth * model.size.x * model.size.y + y * model.size.x + x];
                bottom_voxel.material = WEAK_METAL;
                bottom_voxel.color = color;
                
                auto top_voxel_depth = static_cast<uint32_t>(std::floor(depth_value + 0.5f)) - min_size.z;
                auto& top_voxel = voxels[top_voxel_depth * model.size.x * model.size.y + y * model.size.x + x];
                top_voxel.material = WEAK_METAL;
                top_voxel.color = color;
            }
        }

        model.voxels = std::span(voxels);
        out_model.name = std::format("{}", face_index);
        out_model.size = glm::xzy(model.size);
        out_model.pos = (glm::round(world_min) + glm::vec3(0.5f)) * decimeter_to_meter;

        switch (face.facing) {
            case voxlife::bsp::face::PLANE_X:
            case voxlife::bsp::face::PLANE_ANYX:
                out_model.rot = {-90, -90,   0};
                break;
            case voxlife::bsp::face::PLANE_Y:
            case voxlife::bsp::face::PLANE_ANYY:
                out_model.rot = {  0, +90, +90};
                break;
            case voxlife::bsp::face::PLANE_Z:
            case voxlife::bsp::face::PLANE_ANYZ:
                out_model.rot = {  0,   0,   0};
                break;
        }

        std::filesystem::create_directories(std::format("brush/{}", level_name));

        // std::cout << model.size.x << ", " << model.size.y << ", " << model.size.z << "\n";

        write_magicavoxel_model(std::format("brush/{}/{}.vox", level_name, face_index), std::span(&model, 1));
        return true;
    }

}
