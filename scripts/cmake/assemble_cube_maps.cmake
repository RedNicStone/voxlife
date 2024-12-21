
#[[

This script assembles a cube map from a set of six separate textures.
Teardown uses the DDS format for its cube maps, so we fetch an external tool to generate these.

We fetch the tool from github: https://github.com/dariomanesku/cmft
This is built using make, so make sure you have the required tools installed.

This script takes in the game path and game name and outputs to the specified output directory.

Options:
    -DARG_G <game path>    Path to the game directory
    -DARG_N <game name>    Name of the game
    -DARG_O <output path>  Path to the output directory
    -DARG_C <cubemap name> Name of the cube map to use (custom games only)

Example usage:
cmake -DARG_G="/mnt/int/games/linux/steamapps/common/Half-Life" -DARG_N="hl1" -DARG_O="/home/user/path/to/mod" -P assemble_cube_maps.cmake

cmake -DARG_G="C:/Program Files (x86)/Steam/steamapps/common/Half-Life" -DARG_N="hl1" -DARG_O="C:/Users/user/OneDrive/Documents/Teardown/mods/Mod Testing" -P assemble_cube_maps.cmake

Supported games are:
    hl1
    custom (with -DARG_C <cubemap name> to define which textures to use)

Requirements:
    CMake 3.28 or newer
    Make
    Lua 5.1 or newer
    Git

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

set(CUBEMAP_GAME_PATH   ${ARG_G})
set(CUBEMAP_GAME_NAME   ${ARG_N})
set(CUBEMAP_OUTPUT_PATH ${ARG_O})

if(${CUBEMAP_GAME_NAME} STREQUAL "custom")
    if(NOT DEFINED C)
        message(FATAL_ERROR "Cube map name is required for custom cube maps")
    endif()
    set(CUBEMAP_CUBEMAP_NAME ${ARG_C})
endif()

# Fetch cmft from github
message(STATUS "Fetching cmft from github")

set(CMFT_SOURCE_DIR ${CMAKE_BINARY_DIR}/external/cmft)
if(EXISTS ${CMFT_SOURCE_DIR})
    execute_process(
            COMMAND git -C ${CMFT_SOURCE_DIR} pull
            COMMAND_ERROR_IS_FATAL ANY
            ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE
    )
else()
    execute_process(
            COMMAND git clone https://github.com/dariomanesku/cmft.git ${CMFT_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY
            ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE
    )
endif()

# Build cmft
message(STATUS "Building cmft")
find_program(CMAKE_MAKE_PROGRAM NAMES make mingw32-make)

if (WIN32)
    # For MinGW (Windows)
    # execute_process(
    #         COMMAND ${CMAKE_MAKE_PROGRAM} vs2015
    #         WORKING_DIRECTORY ${CMFT_SOURCE_DIR}
    #         COMMAND_ERROR_IS_FATAL ANY
    #         ECHO_ERROR_VARIABLE
    # )
    # set(CUBEMAP_DDS_COMMAND ${CMFT_SOURCE_DIR}/_build/vs2015/bin/cmftRelease.exe)
    set(CUBEMAP_DDS_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cmft.exe)
elseif(UNIX)
    # For Linux
    execute_process(
            COMMAND ${CMAKE_MAKE_PROGRAM} linux-release64
            WORKING_DIRECTORY ${CMFT_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY
            ECHO_ERROR_VARIABLE
    )
    set(CUBEMAP_DDS_COMMAND ${CMFT_SOURCE_DIR}/_build/linux64_gcc/bin/cmftRelease)
endif()

# Enumerate the cube map textures
if(${CUBEMAP_GAME_NAME} STREQUAL "hl1")
    set(CUBEMAP_TEXTURES
            "alien1"
            "alien2"
            "alien3"
            "black"
            "city"
            "cliff"
            "desert"
            "2desert"
            "dusk"
            "morning"
            "neb1"
            "neb2"
            "neb6"
            "neb7"
            "night"
            "space"
            "xen8"
            "xen9"
            "xen10"
    )
    set(CUBEMAP_TEXTURE_PREFIX "valve/gfx/env/")
elseif(${CUBEMAP_GAME_NAME} STREQUAL "custom")
    set(CUBEMAP_TEXTURES CUBEMAP_CUBEMAP_NAME)
    set(CUBEMAP_TEXTURE_PREFIX)
else()
    message(FATAL_ERROR "Unsupported game")
endif()

# Set parameters
set(CUBEMAP_DDS_FILTERS
    --filter radiance
    --srcFaceSize 256
    --excludeBase true
    --mipCount 20
    --glossScale 12
    --glossBias 3
    --lightingModel blinnbrdf
    --edgeFixup none
    --dstFaceSize 256
    # ::Processing devices
    --numCpuProcessingThreads 4
    --useOpenCL false
    --clVendor anyGpuVendor
    --deviceType gpu
    --deviceIndex 0
    # ::Aditional operations
    --inputGammaNumerator 2.2
    --inputGammaDenominator 1.0
    --outputGammaNumerator 1.0
    --outputGammaDenominator 1.0
    --generateMipChain false
    --outputNum 1
    --output0params dds,rgba32f,cubemap
    # Teardown-specific... It doesn't work still. Seems like it might be map dependent :/
    --negYrotate180
    --posYrotate180
)

# Generate the cube map
message(STATUS "Generating cube maps")
message(STATUS "Cube map parameters: ${CUBEMAP_DDS_COMMAND}")

foreach(TEXTURE ${CUBEMAP_TEXTURES})
    set(CUBEMAP_DDS_OUTPUT ${CUBEMAP_OUTPUT_PATH}/${TEXTURE})
    set(CUBEMAP_TEXTURE_PATH ${CUBEMAP_GAME_PATH}/${CUBEMAP_TEXTURE_PREFIX}${TEXTURE})

    set(CUBEMAP_DDS_PARAMETERS ${CUBEMAP_DDS_FILTERS}
            --inputFacePosX ${CUBEMAP_TEXTURE_PATH}bk.tga
            --inputFaceNegX ${CUBEMAP_TEXTURE_PATH}ft.tga
            --inputFacePosY ${CUBEMAP_TEXTURE_PATH}up.tga
            --inputFaceNegY ${CUBEMAP_TEXTURE_PATH}dn.tga
            --inputFacePosZ ${CUBEMAP_TEXTURE_PATH}lf.tga
            --inputFaceNegZ ${CUBEMAP_TEXTURE_PATH}rt.tga
            --output0 ${CUBEMAP_DDS_OUTPUT}
    )

    message(STATUS "> ${TEXTURE}")
    execute_process(
            COMMAND ${CUBEMAP_DDS_COMMAND} ${CUBEMAP_DDS_PARAMETERS}
            ECHO_ERROR_VARIABLE
    )
endforeach()

