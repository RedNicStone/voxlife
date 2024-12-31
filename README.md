# Vox-Life

A Half-Life Teardown mod, where much of the data is generated from the original Half-Life game files.

<br/><img src="res/cliff.png" width="50%"><br/>
Gordon on the Cliff map (c2a5a)

 - Why did you write your own code to extract the mesh data from the maps? Tools for this already exist, and so you could just use VoxTool from there!
<br/>A few reasons. Firstly, this is my version of "fun". But also, there are practical reasons. This way, I can extract the light positions, NPCs, skyboxes, etc. all automatically! And I don't need to click through multiple GUIs for all 100 maps 🙏

Here's an example of that nice extracted lighting, with dynamic shadows now, because Teardown does lighting in real-time!
<br/><img src="res/lambda-core.png" width="50%"><br/>
Gordon in Lambda Core

I also have a bunch of custom hand-made characters, as well as guns coming soon!
<br/><img src="res/gordon-barney-gman.png" height="200px"><img src="res/scientists.png" height="200px"><img src="res/marines.png" height="200px"><br/>
Gordon, Barney, G-Man, Scientists, and Marines

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
