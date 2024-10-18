
cmake_minimum_required(VERSION 3.28)
project(voxlife)

set(CMAKE_CXX_STANDARD 23)

# todo: Ofast may cause issues, more testing is needed
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=native -mtune=native -fopenmp")

add_subdirectory(lib)
add_subdirectory(src)
