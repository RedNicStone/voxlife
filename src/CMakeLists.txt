
file(GLOB_RECURSE SRC *.cpp)

add_executable(voxlife ${SRC})

target_include_directories(voxlife
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
