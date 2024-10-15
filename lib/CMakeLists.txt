
add_library(opengametools INTERFACE)

target_include_directories(opengametools
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/opengametools
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/glm)
