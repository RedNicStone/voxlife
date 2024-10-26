
#[[

This script stitches textures into a single image.
Its designed to stick the menu textures from HL1 into a single image.

This script takes in the game path and game name and outputs to the specified output directory.

Options:
    -DARG_G <game path>    Path to the game directory
    -DARG_N <game name>    Name of the game
    -DARG_O <output path>  Path to the output directory

Example usage:
cmake -DARG_G="/mnt/int/games/linux/steamapps/common/Half-Life" -DARG_N="hl1" -DARG_O="/home/user/path/to/mod/file.png" -P assemble_images.cmake

Supported games are:
    hl1

Requirements:
    CMake 3.28 or newer
    ImageMagick

]]

# Parse the command line arguments
if(NOT DEFINED ARG_G)
    message(FATAL_ERROR "Game path is required")
endif()

if(NOT DEFINED ARG_N)
    message(FATAL_ERROR "Game name is required")
endif()

if(NOT DEFINED ARG_O)
    message(FATAL_ERROR "Output path is required")
endif()

set(STITCH_GAME_PATH   ${ARG_G})
set(STITCH_GAME_NAME   ${ARG_N})
set(STITCH_OUTPUT_PATH ${ARG_O})

# Enumerate the cube map textures
if(${STITCH_GAME_NAME} STREQUAL "hl1")
    foreach(ROW RANGE 1 7)
        set(STITCH_TEXTURES_ROW)
        foreach(COLUMN a;b;c;d;e;f;g;h;i;j;k;l;m;n;o)
            list(APPEND STITCH_TEXTURES_ROW "21_9_${ROW}_${COLUMN}_loading.tga")
        endforeach()
        list(APPEND STITCH_TEXTURES "${STITCH_TEXTURES_ROW}")
    endforeach()
    set(STITCH_TEXTURE_PREFIX "/valve/resource/background/")
else()
    message(FATAL_ERROR "Unsupported game")
endif()

find_program(STITCH_COMMAND NAMES montage)
if(NOT STITCH_COMMAND)
    message(FATAL_ERROR "Could not find imagemagick's 'montage'")
endif()
message(STATUS "ImageMagick found at ${STITCH_COMMAND}")

# Set parameters
set(STITCH_FILTERS -mode Concatenate -tile 15x7 -background none)

# Generate the cube map
message(STATUS "Stitching texture")

set(STITCH_TEXTURE_OUTPUT ${STITCH_OUTPUT_PATH})

set(STITCH_PARAMETERS)
foreach(ROWS ${STITCH_TEXTURES})
    foreach(TEXTURE ${ROWS})
        list(APPEND STITCH_PARAMETERS ${TEXTURE})
    endforeach()
endforeach()

list(APPEND STITCH_PARAMETERS ${STITCH_FILTERS} ${STITCH_OUTPUT_PATH})

message(STATUS "> ${STITCH_OUTPUT_PATH}")
execute_process(
        COMMAND ${STITCH_COMMAND} ${STITCH_PARAMETERS}
        WORKING_DIRECTORY ${STITCH_GAME_PATH}${STITCH_TEXTURE_PREFIX}
        COMMAND_ERROR_IS_FATAL ANY
        ECHO_ERROR_VARIABLE
        ECHO_OUTPUT_VARIABLE
)

