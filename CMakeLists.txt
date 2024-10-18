
cmake_minimum_required(VERSION 3.28)
project(voxlife)

set(CMAKE_CXX_STANDARD 23)

# todo: Ofast may cause issues, more testing is needed
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=native -mtune=native -fopenmp")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_CXX_FLAGS "/openmp:experimental")
    else()
        message(FATAL_ERROR "unsupported compiler")
    endif()
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=native -mtune=native -fopenmp")
endif()

add_subdirectory(lib)
add_subdirectory(src)
