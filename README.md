Voxel game in OpenGL
Relies on Monolithic Unit Tests

- [Building](#building)
  - [Msvc](#building-msvc)
  - [GCC/Clang](#building-gcc-clang)

# Building <a id="building"></a>
Uses [nob.h](https://github.com/tsoding/nob.h)
## MSVC <a id="building-msvc"></a>
Note: first line is only needed once, the build system rebuilds itself after
```cmd
cl nob.c
nob
bin\vonu
```
Note: when running the application, your working directory has to be the one with the assets in it

## GCC/Clang <a id="building-gcc-clang"></a>

Note: first line is only needed once, the build system rebuilds itself after
```bash
cc nob.c -o nob
./nob
./bin/vonu 
```
Note: when running the application, your working directory has to be the one with the assets in it
