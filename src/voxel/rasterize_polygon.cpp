
#include "rasterize_polygon.h"

#include <utils/wrapping_iterator.h>

#include <optional>
#include <vector>
#include <numeric>
#include <cmath>
#include <chrono>
#include <iostream>
#include <ranges>
#include <fstream>


using namespace voxlife::voxel;

struct varying_occupancy : public varying_base {
    std::vector<bool> grid;

    varying_occupancy() = delete;
    explicit varying_occupancy(const voxlife::voxel::grid_properties& grid_info) : varying_base(grid_info), grid(grid_info.width * grid_info.height) {}
    ~varying_occupancy() override = default;

    void interpolate_row(const interpolation_info &info) override;
};


namespace voxlife::voxel {

    void create_rasterizer(const grid_properties &grid_info, rasterizer_handle *handle) {
        *handle = reinterpret_cast<rasterizer_handle>(new rasterizer(grid_info));
    }

    void create_occupancy_varying(rasterizer_handle rasterizer, varying_handle *handle) {
        auto &rasterizer_ref = *reinterpret_cast<struct rasterizer *>(rasterizer);

        varying_base *varying = new varying_occupancy(rasterizer_ref.grid_info);
        rasterizer_ref.varyings.push_back(varying);

        *handle = reinterpret_cast<varying_handle>(varying);
    }

    void rasterize_polygon(rasterizer_handle rasterizer, std::span<glm::vec2> points) {
        auto &rasterizer_ref = *reinterpret_cast<struct rasterizer *>(rasterizer);

        rasterizer_ref.rasterize_polygon(points);
    }

    /*std::span<bool> get_occupancy_varying_grid(varying_handle varying) {
        auto& varying_ref = *reinterpret_cast<struct varying_occupancy*>(varying);

        return std::span(varying_ref.grid);
    }*/

}


/// Intersects a horizontal ray with a line segment
/// \param ray_y vertical position of the ray
/// \param p1 Point 1 on the line segment
/// \param p2 Point 2 on the line segment
/// \return Horizontal position of the intersection, or std::nullopt if no intersection
std::optional<float> intersectRayWithLine(float ray_y, const glm::vec2& p1, const glm::vec2& p2) {
    if (p1.y == p2.y)
        return std::nullopt;

    float s = (ray_y - p1.y) / (p2.y - p1.y);
    float x_intersect = p1.x + s * (p2.x - p1.x);

    return x_intersect;
}


void varying_occupancy::interpolate_row(const interpolation_info &info) {
    auto line_begin = grid.begin() + info.line_y * grid_info.width;

    std::fill(line_begin + info.front_intersection_index, line_begin + info.back_intersection_index, true);
}

void rasterizer::rasterize_polygon(std::span<glm::vec2> points) {
    std::vector<std::pair<uint32_t, uint32_t>> line_indices;
    line_indices.resize(points.size());

    // Determine the signed area of the polygon
    float signed_area = 0.0f;
    for (auto it = points.begin(); it != points.end() - 1; ++it) {
        auto first_point = *it;
        auto second_point = *(it + 1);

        signed_area += first_point.x * second_point.y - second_point.x * first_point.y;
    }
    signed_area += points.back().x * points.front().y - points.front().x * points.back().y;

    // Reverse the line indices if the winding order is CCW
    if (signed_area < 0.0f) {
        // Generate forward line indices
        std::generate(line_indices.begin(), line_indices.end() - 1,
                      [index = 0ul]() mutable -> std::pair<uint32_t, uint32_t> {
                          index++;
                          return {index - 1, index};
                      });
        line_indices.back() = { points.size() - 1, 0 };
    } else {
        // Generate backward line indices
        line_indices.front() = { 0, points.size() - 1 };
        std::generate(line_indices.begin() + 1, line_indices.end(),
                      [index = points.size()]() mutable -> std::pair<uint32_t, uint32_t> {
                          index--;
                          return { index, index - 1 };
                      });
    }

    // Remove points that are on the same line
    line_indices.erase(std::remove_if(line_indices.begin(), line_indices.end(),
                                      [&](const auto pair) {
        auto first_point = points[pair.first];
        auto second_point = points[pair.second];

        if (first_point.y == second_point.y)
            return true;

        return false;
    }), line_indices.end());

    // Find the lowest and highest point in the polygon for the front section
    const auto [min_first_point, max_first_point] = std::minmax_element(line_indices.begin(), line_indices.end(),
                                                                        [&](const std::pair<uint32_t, uint32_t> &a,
                                                                            const std::pair<uint32_t, uint32_t> &b) {
        return points[a.first].y < points[b.first].y;
    });

    // Find the lowest and highest point in the polygon for the front section
    const auto [min_second_point, max_second_point] = std::minmax_element(line_indices.begin(), line_indices.end(),
                                                                          [&](const std::pair<uint32_t, uint32_t> &a,
                                                                              const std::pair<uint32_t, uint32_t> &b) {
        return points[a.second].y < points[b.second].y;
    });

    const float min_y = points[min_first_point->first].y;
    const float max_y = points[max_first_point->first].y;

    // Number of scan lines needed to cover the polygon
    const uint32_t iterations_y = std::min(static_cast<uint32_t>(std::round(max_y) - std::round(min_y)),
                                           grid_info.height);

    // The minimum and maximum area required to cover the polygon
    const float grid_min_x = grid_info.origin.x + 0.5f;
    const float grid_min_y = std::max(std::round(min_y) + 0.5f, std::round(grid_info.origin.y) + 0.5f);
    const auto grid_width_minus_one = static_cast<float>(grid_info.width - 1);

    // This iterator can iterate in either direction, wrapping at the end of the range
    using forward_wrapping_view = forward_wrapping_view<decltype(line_indices)::iterator>;
    using reverse_wrapping_view = reverse_wrapping_view<decltype(line_indices)::iterator>;

    // Iterator for the front-facing lines of the polygon
    const auto front_view = forward_wrapping_view(min_first_point, max_first_point, line_indices.begin(),
                                                  line_indices.end());
    auto front_iterator = front_view.begin();

    // Iterator for the back-facing lines of the polygon
    const auto back_view = reverse_wrapping_view(min_second_point, max_second_point, line_indices.begin(),
                                                 line_indices.end());
    auto back_iterator = back_view.begin();

    auto front_indices = *front_iterator;
    auto back_indices = *back_iterator;
    float front_length_y = points[front_indices.second].y - points[front_indices.first].y;
    float back_length_y = points[back_indices.first].y - points[back_indices.second].y;
    for (uint32_t y = 0; y < iterations_y; ++y) {
        const float absolute_y = static_cast<float>(y) + grid_min_y;

        if (points[(*front_iterator).second].y < absolute_y) {
            ++front_iterator;
            front_indices = *front_iterator;

            front_length_y = points[front_indices.second].y - points[front_indices.first].y;
        }

        if (points[(*back_iterator).first].y < absolute_y) {
            ++back_iterator;
            back_indices = *back_iterator;

            back_length_y = points[back_indices.first].y - points[back_indices.second].y;
        }

        const auto front_intersection = intersectRayWithLine(absolute_y, points[front_indices.first],
                                                             points[front_indices.second]);
        const auto back_intersection = intersectRayWithLine(absolute_y, points[back_indices.second],
                                                            points[back_indices.first]);

        if (!front_intersection || !back_intersection)
            throw std::runtime_error("Intersection is null");

        const float line_front = front_intersection.value() - grid_min_x;
        const float line_back = back_intersection.value() - grid_min_x;

        const auto line_min = static_cast<uint32_t>(std::min(std::max(line_front, 0.0f), grid_width_minus_one));
        const auto line_max = static_cast<uint32_t>(std::min(std::max(line_back, 0.0f), grid_width_minus_one));

        if (line_min > line_max) [[unlikely]]  // This should never happen in a convex polygon
            throw std::runtime_error("Line min is greater than line max");

        interpolation_info info{};
        info.front_point_1_index = front_indices.first;
        info.front_point_2_index = front_indices.second;
        info.back_point_1_index = back_indices.second;
        info.back_point_2_index = back_indices.first;
        info.line_y = y;
        info.relative_front_y = (absolute_y - points[front_indices.first].y) / front_length_y;
        info.relative_back_y = (absolute_y - points[back_indices.second].y) / back_length_y;
        info.front_intersection = line_front;
        info.back_intersection = line_back;
        info.front_intersection_index = line_min;
        info.back_intersection_index = line_max;

        for (auto &varying: varyings)
            varying->interpolate_row(info);
    }
}

/*
void raster_test() {
    std::vector<glm::vec2> points{};
    std::vector<float> depth;
    std::vector<glm::vec2> uvs;

    size_t point_count = 20;
    float radius = 500.0f;
    points.reserve(point_count);
    depth.reserve(point_count);
    for (int i = 0; i < point_count; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(point_count) * 2.0f * M_PIf;
        float x = std::sin(angle) * -radius;
        float y = std::cos(angle) * -radius;
        points.emplace_back(x, y);

        depth.emplace_back(std::sin(angle * 2));
        uvs.emplace_back(std::sin(angle * 2), std::cos(angle * 2));
    }

    grid_properties grid_info{};
    grid_info.width = static_cast<uint32_t>(radius * 2);
    grid_info.height = static_cast<uint32_t>(radius * 2);
    grid_info.origin = glm::vec2{-radius, -radius};

    rasterizer_handle rasterizer;
    create_rasterizer(grid_info, &rasterizer);

    //varying_handle v_occ;
    //create_occupancy_varying(rasterizer, &v_occ);
    varying_handle v_depth;
    create_smooth_varying(rasterizer, std::span(depth), &v_depth);
    varying_handle v_uv;
    create_smooth_varying(rasterizer, std::span(uvs), &v_uv);

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    rasterize_polygon(rasterizer, std::span(points));
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    std::cout << "Grid size: " << grid_info.width << " x " << grid_info.height << std::endl;
    std::chrono::duration<float, std::micro> elapsed = end - start;
    std::cout << "Rasterization took: " << elapsed << std::endl;
    std::chrono::duration<float, std::nano> elapsed_pixel = (end - start) / static_cast<float>(grid_info.width * grid_info.height);
    std::cout << "Per pixel: " << elapsed_pixel  << std::endl;

    auto depth_data = voxlife::voxel::get_smooth_varying_grid<float>(v_depth);

    auto file = std::ofstream("depth.ppm", std::ios::out);
    file << "P2\n";
    file << grid_info.width << " " << grid_info.height << "\n";
    file << "255\n";

    for (auto x = 0; x < grid_info.width; ++x) {
        for (auto y = 0; y < grid_info.height; ++y) {
            auto d = depth_data[y * grid_info.width + x];
            file << +static_cast<uint8_t>((d + 1.0f) / 2.0f * 255.0f) << ' ';
        }
        file << '\n';
    }
    file.close();

    auto uv_data = voxlife::voxel::get_smooth_varying_grid<glm::vec2>(v_uv);

    file = std::ofstream("uv.ppm", std::ios::out);
    file << "P3\n";
    file << grid_info.width << " " << grid_info.height << "\n";
    file << "255\n";

    for (auto x = 0; x < grid_info.width; ++x) {
        for (auto y = 0; y < grid_info.height; ++y) {
            auto d = uv_data[y * grid_info.width + x];
            file << +static_cast<uint8_t>((d.x + 1.0f) / 2.0f * 255.0f) << ' ';
            file << +static_cast<uint8_t>((d.y + 1.0f) / 2.0f * 255.0f) << ' ';
            file << "0\n";
        }
    }
    file.close();
}
 */
