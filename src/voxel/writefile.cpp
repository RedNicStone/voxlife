#define OGT_VOX_IMPLEMENTATION
#include <ogt_vox.h>

#include <voxel/writefile.h>

#include <vector>
#include <cstdio>

#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <set>
#include <format>
#include <span>
#include <array>
#include <algorithm>

auto rgb_to_oklab(std::array<double, 3> rgb) -> std::array<double, 3> {
    // Normalize the RGB values to the range [0, 1]
    for (int i = 0; i < 3; ++i)
        rgb[i] = rgb[i] / 255.0;
    // Convert to the XYZ color space
    std::array<double, 3> xyz;
    xyz[0] = 0.4124564 * rgb[0] + 0.3575761 * rgb[1] + 0.1804375 * rgb[2];
    xyz[1] = 0.2126729 * rgb[0] + 0.7151522 * rgb[1] + 0.0721750 * rgb[2];
    xyz[2] = 0.0193339 * rgb[0] + 0.1191920 * rgb[1] + 0.9503041 * rgb[2];
    // Normalize XYZ
    double x = xyz[0] / 0.95047f; // D65 white point
    double y = xyz[1] / 1.0f;
    double z = xyz[2] / 1.08883f;
    // Convert to Oklab
    double l = 0.210454 * x + 0.793617 * y - 0.004072 * z;
    double a = 1.977665 * x - 0.510530 * y - 0.447580 * z;
    double b = 0.025334 * x + 0.338572 * y - 0.602190 * z;
    return {l, a, b};
}

auto oklab_to_rgb(std::array<double, 3> oklab) -> std::array<double, 3> {
    // Convert to XYZ
    std::array<double, 3> xyz;
    xyz[0] = +0.44562442079 * oklab[0] + 0.46266924383 * oklab[1] - 0.34689397498 * oklab[2];
    xyz[1] = +1.14528157354 * oklab[0] - 0.12294697715 * oklab[1] + 0.08363642948 * oklab[2];
    xyz[2] = +0.66266414585 * oklab[0] - 0.04966064087 * oklab[1] - 1.62817592248 * oklab[2];
    // Un-normalize XYZ
    double x = xyz[0] * 0.95047f; // D65 white point
    double y = xyz[1] * 1.0f;
    double z = xyz[2] * 1.08883f;
    // Convert to the RGB color space
    std::array<double, 3> rgb;
    rgb[0] = 3.2404542 * x + -1.5371385 * y + -0.4985314 * z;
    rgb[1] = -0.9692660 * x + 1.8760108 * y + 0.0415560 * z;
    rgb[2] = 0.0556434 * x + -0.2040259 * y + 1.0572252 * z;
    return rgb;
}

struct Point {
    std::array<double, 3> coordinates;
    int cluster_id; // Assigned cluster ID
    Point(const std::array<double, 3> &coords, int id = -1) : coordinates(coords), cluster_id(id) {}
};

// Function to compute the squared Euclidean distance between two points
double squaredEuclideanDistance(const Point &a, const Point &b) {
    double distance = 0.0;
    for (size_t i = 0; i < a.coordinates.size(); ++i) {
        double diff = a.coordinates[i] - b.coordinates[i];
        distance += diff * diff;
    }
    return distance;
}
// Function to compute the Euclidean distance between two points
double euclideanDistance(const Point &a, const Point &b) {
    return std::sqrt(squaredEuclideanDistance(a, b));
}

void hierarchicalClustering(std::vector<Point> &points, int k) {
    int n = points.size();
    if (n == 0 || k <= 0 || k > n)
        return;

    // Initialize each point to its own cluster
    for (int i = 0; i < n; ++i) {
        points[i].cluster_id = i;
    }

    // Precompute the distance matrix
    std::vector<std::vector<double>> distance_matrix(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dist = euclideanDistance(points[i], points[j]);
            distance_matrix[i][j] = dist;
            distance_matrix[j][i] = dist;
        }
    }

    // Clusters represented as sets of point indices
    std::vector<std::set<int>> clusters(n);
    for (int i = 0; i < n; ++i) {
        clusters[i].insert(i);
    }

    int current_cluster_count = n;
    while (current_cluster_count > k) {
        double min_distance = std::numeric_limits<double>::max();
        int cluster_a = -1, cluster_b = -1;

        // Find the two closest clusters
        for (int i = 0; i < n; ++i) {
            if (clusters[i].empty())
                continue;
            for (int j = i + 1; j < n; ++j) {
                if (clusters[j].empty())
                    continue;

                // Use single linkage (minimum distance between clusters)
                double dist = std::numeric_limits<double>::max();
                for (int p1 : clusters[i]) {
                    for (int p2 : clusters[j]) {
                        dist = std::min(dist, distance_matrix[p1][p2]);
                    }
                }

                if (dist < min_distance) {
                    min_distance = dist;
                    cluster_a = i;
                    cluster_b = j;
                }
            }
        }

        // Merge the two closest clusters
        if (cluster_a != -1 && cluster_b != -1) {
            clusters[cluster_a].insert(clusters[cluster_b].begin(), clusters[cluster_b].end());
            clusters[cluster_b].clear();
            current_cluster_count--;
        } else {
            break; // No more clusters can be merged
        }
    }

    // Assign cluster IDs based on the clusters formed
    int cluster_id = 0;
    for (const auto &cluster : clusters) {
        if (cluster.empty())
            continue;
        for (int idx : cluster) {
            points[idx].cluster_id = cluster_id;
        }
        cluster_id++;
    }
}

void kMeans(std::vector<Point> &points, int k, int max_iterations = 100) {
    if (points.empty() || k <= 0)
        return;

    size_t dimensions = points[0].coordinates.size();
    std::vector<Point> centroids;

    // Initialize centroids by randomly selecting k unique points from the dataset
    // std::srand(std::time(0));
    std::vector<int> used_indices;
    for (int i = 0; i < k; ++i) {
        int idx;
        do {
            idx = std::rand() % points.size();
        } while (std::find(used_indices.begin(), used_indices.end(), idx) != used_indices.end());
        used_indices.push_back(idx);
        centroids.push_back(points[idx]);
    }

    bool changed;
    int iterations = 0;
    do {
        changed = false;

        // Assignment step: Assign each point to the nearest centroid
        for (auto &point : points) {
            double min_distance = std::numeric_limits<double>::max();
            int closest_cluster = -1;
            for (int i = 0; i < k; ++i) {
                double distance = squaredEuclideanDistance(point, centroids[i]);
                if (distance < min_distance) {
                    min_distance = distance;
                    closest_cluster = i;
                }
            }
            if (point.cluster_id != closest_cluster) {
                point.cluster_id = closest_cluster;
                changed = true;
            }
        }

        // Update step: Recalculate centroids as the mean of assigned points
        std::vector<std::array<double, 3>> new_centroids(k, std::array<double, 3>{});
        std::vector<int> points_per_cluster(k, 0);
        for (const auto &point : points) {
            int cluster = point.cluster_id;
            for (size_t d = 0; d < dimensions; ++d) {
                new_centroids[cluster][d] += point.coordinates[d];
            }
            points_per_cluster[cluster] += 1;
        }

        // Avoid division by zero and update centroids
        for (int i = 0; i < k; ++i) {
            if (points_per_cluster[i] == 0)
                continue;
            for (size_t d = 0; d < dimensions; ++d) {
                new_centroids[i][d] /= points_per_cluster[i];
            }
            centroids[i].coordinates = new_centroids[i];
        }

        ++iterations;
    } while (changed && iterations < max_iterations);
}

auto generate_palette(std::span<VoxelModel> in_models) -> std::pair<ogt_vox_palette, std::vector<std::vector<uint8_t>>> {
    auto result = std::pair<ogt_vox_palette, std::vector<std::vector<uint8_t>>>{};
    auto &[palette, model_voxels] = result;

    std::vector<Point> points;

    for (auto const &model : in_models) {
        points.reserve(points.size() + model.voxels.size());
        for (auto const &voxel : model.voxels) {
            auto a = (voxel >> 24) & 0xff;
            if (a == 0)
                continue;
            auto pt = std::array<double, 3>{};
            pt[0] = static_cast<double>((voxel >> 0) & 0xff);
            pt[1] = static_cast<double>((voxel >> 8) & 0xff);
            pt[2] = static_cast<double>((voxel >> 16) & 0xff);
            points.push_back(Point(rgb_to_oklab(pt)));
        }
    }

    kMeans(points, 32);

    model_voxels.resize(in_models.size());
    uint32_t point_i = 0;

    std::array<std::array<double, 4>, 32> sums{};

    for (uint32_t model_i = 0; model_i < in_models.size(); ++model_i) {
        auto const &model = in_models[model_i];
        auto &voxels = model_voxels[model_i];

        voxels.resize(model.size_x * model.size_y * model.size_z);

        for (uint32_t voxel_i = 0; voxel_i < model.voxels.size(); ++voxel_i) {
            auto const &voxel = model.voxels[voxel_i];
            auto a = (voxel >> 24) & 0xff;
            if (a == 0) {
                voxels[voxel_i] = 0;
                continue;
            }
            auto const &point = points[point_i];
            voxels[voxel_i] = point.cluster_id + 1;

            auto &sum = sums[point.cluster_id];
            sum[0] += point.coordinates[0]; // static_cast<double>((voxel >> 0) & 0xff);
            sum[1] += point.coordinates[1]; // static_cast<double>((voxel >> 8) & 0xff);
            sum[2] += point.coordinates[2]; // static_cast<double>((voxel >> 16) & 0xff);
            sum[3] += 1;

            ++point_i;
        }
    }

    for (uint32_t in_palette_i = 0; in_palette_i < sums.size(); ++in_palette_i) {
        auto &sum = sums[in_palette_i];
        sum[0] /= sum[3];
        sum[1] /= sum[3];
        sum[2] /= sum[3];
        auto &color = palette.color[in_palette_i + 1];
        auto avg = oklab_to_rgb({sum[0], sum[1], sum[2]});
        color.r = std::clamp(avg[0] * 255.0, 0.0, 255.0);
        color.g = std::clamp(avg[1] * 255.0, 0.0, 255.0);
        color.b = std::clamp(avg[2] * 255.0, 0.0, 255.0);
    }

    return result;
}

void write_magicavoxel_model(std::string_view filename, std::span<VoxelModel> in_models) {
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
        model->size_x = in_models[i].size_x;
        model->size_y = in_models[i].size_y;
        model->size_z = in_models[i].size_z;
        model->voxel_data = voxels[i].data();
        models[i] = model;

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

    for (auto const *model : models)
        delete model;
}
