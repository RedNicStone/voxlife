
file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(voxlife ${SRC})

target_include_directories(voxlife
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(daxa CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

target_link_libraries(voxlife
    PRIVATE
        opengametools
        glm::glm
        daxa::daxa
        glfw
        imguizmo::imguizmo
)
