#pragma once

#include <bsp/read_file.h>

void test_voxelization_gui(voxlife::bsp::bsp_handle handle);
void voxelize_gpu(voxlife::bsp::bsp_handle handle, std::string_view level_name, std::vector<struct Model> &models);
