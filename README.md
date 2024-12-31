# Vox-Life

A Half-Life Teardown mod, where much of the data is generated from the original Half-Life game files.

Building the skyboxes
```
cd scripts/cmake
cmake -DARG_G="<Half-Life Game Dir>" -DARG_N="hl1" -P assemble_cube_maps.cmake
```

Building the level geometry and map XML files (used by `changelevel.lua`)
```
# compile the code...
cd scripts/teardown-mod
voxlife.exe "<Half-Life Game Dir>" all
```
(Or just press F5 in vscode)

This project uses C++/CMake/vcpkg
