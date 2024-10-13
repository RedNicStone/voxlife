
file(GLOB_RECURSE SRC *.cpp)

add_executable(voxelife ${SRC})

target_include_directories(voxelife
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
