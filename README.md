Voxel game in OpenGL
Relies on Monolithic Unit Tests

- [Building](#building)
  - [Msvc](#building-msvc)
  - [GCC/Clang](#building-gcc-clang)

# Building <a id="building"></a>
## MSVC <a id="building-msvc"></a>
Note: first line is only needed once, the build system rebuilds itself after
```cmd
cl nob.c
nob
bin\vonu
```
Note: your working directory has to be the one with the assets in it

## GCC/Clang <a id="building-gcc-clang"></a>

```bash
cc nob.c -o nob # this line is only needed once, the build system rebuilds itself after
./nob
./bin/vonu # your working directory has to be the one with the assets in it
```
