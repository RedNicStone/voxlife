
#ifndef VOXLIFE_RASTERIZE_POLYGON_H
#define VOXLIFE_RASTERIZE_POLYGON_H

#include <cstdint>
#include <span>
#include <vector>


namespace voxlife::voxel {

    struct vec2f32 {
        float x, y;
    };

    struct grid_properties {
        uint32_t width, height;
        vec2f32 origin;
    };

}

namespace {

    struct interpolation_info {
        uint32_t front_point_1_index, front_point_2_index;
        uint32_t back_point_1_index, back_point_2_index;
        uint32_t line_y;
        float relative_front_y, relative_back_y;
        float front_intersection, back_intersection;
        uint32_t front_intersection_index, back_intersection_index;
    };

    struct varying_base {
        voxlife::voxel::grid_properties grid_info{};

        varying_base() = delete;
        explicit varying_base(voxlife::voxel::grid_properties info) : grid_info(info) {}
        virtual ~varying_base() = default;

        virtual void interpolate_row(const interpolation_info& info) = 0;
    };

    template<typename T>// requires std::is_arithmetic_v<T>
    struct varying_smooth : public varying_base {
        std::span<T> points;
        std::vector<T> grid;

        varying_smooth() = delete;
        explicit varying_smooth(const voxlife::voxel::grid_properties& grid_info, std::span<T> data) : varying_base(grid_info), grid(grid_info.width * grid_info.height), points(data) {}
        ~varying_smooth() override = default;

        void interpolate_row(const interpolation_info &info) override {
            auto& font_point_1 = points[info.front_point_1_index];
            auto& font_point_2 = points[info.front_point_2_index];
            auto& back_point_1 = points[info.back_point_1_index];
            auto& back_point_2 = points[info.back_point_2_index];

            T font_value = font_point_1 + (font_point_2 - font_point_1) * info.relative_front_y;
            T back_value = back_point_1 + (back_point_2 - back_point_1) * info.relative_back_y;

            float faction_inverse_x = 1.0f / (info.back_intersection - info.front_intersection);
            T incremental_value = (back_value - font_value) * faction_inverse_x;
            T value = font_value + incremental_value * (static_cast<float>(info.front_intersection_index) - info.front_intersection);
            for (auto x = info.front_intersection_index; x < info.back_intersection_index; ++x) {
                value += incremental_value;
                grid[info.line_y * grid_info.width + x] = value;
            }
        }
    };

    struct rasterizer {
        voxlife::voxel::grid_properties grid_info{};
        std::vector<varying_base*> varyings{};

        void rasterize_polygon(std::span<voxlife::voxel::vec2f32> points);
    };

}

namespace voxlife::voxel {

    typedef struct rasterizer_handle_T* rasterizer_handle;
    typedef struct varying_handle_T* varying_handle;

    void create_rasterizer(const grid_properties& grid_info, rasterizer_handle* handle);

    void create_occupancy_varying(rasterizer_handle rasterizer, varying_handle* handle);

    template<typename T>
    void create_smooth_varying(rasterizer_handle rasterizer, std::span<T> points, varying_handle* handle) {
        auto& rasterizer_ref = *reinterpret_cast<struct rasterizer*>(rasterizer);

        varying_base* varying = new varying_smooth<T>(rasterizer_ref.grid_info, points);
        rasterizer_ref.varyings.push_back(varying);

        *handle = reinterpret_cast<varying_handle>(varying);
    }

    void rasterize_polygon(rasterizer_handle rasterizer, std::span<vec2f32> points);

    std::span<bool> get_occupancy_varying_grid(varying_handle varying);  // todo: NOP
    template<typename T>
    std::span<T> get_smooth_varying_grid(varying_handle varying) {
        auto& varying_ref = *reinterpret_cast<struct varying_smooth<T>*>(varying);

        return std::span(varying_ref.grid);
    }

    void destroy_rasterizer(rasterizer_handle rasterizer);  // this will destroy all varyings

}

#endif //VOXLIFE_RASTERIZE_POLYGON_H
