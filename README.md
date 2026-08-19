# minecraft

A Minecraft-style voxel renderer written from scratch in C++ with OpenGL 3.3.
It generates an infinite-feeling world from Perlin and cellular noise, meshes it
into chunks, and lets you fly around it.

## Features

- Procedural terrain from a seeded noise stack (height map, gravel, dirt)
- 16 x 255 x 16 chunks, meshed with interior faces culled
- Chunk streaming: chunks load and unload as you move
- Multithreaded chunk generation across the available cores
- Texture atlas sampling for per-block, per-face textures
- Free-fly camera with mouse look

## Building

You need a C++ compiler with C++11 support and GNU make.

### Windows (MinGW / TDM-GCC)

GLFW is already vendored in `lib/`, so no extra setup is needed:

```bash
make
```

### Linux

```bash
sudo apt install libglfw3-dev
make
```

### macOS

```bash
brew install glfw
make
```

The Makefile detects the platform and picks the right link flags. There is no
longer anything to paste in by hand.

## Running

Run from the project root, so the shader and texture paths resolve:

```bash
./app
```

The seed is printed to the console at startup.

## Controls

| Key | Action |
| --- | --- |
| `W` `A` `S` `D` | Move horizontally |
| `Space` | Move up |
| `Left Shift` | Move down |
| Mouse | Look around |
| `Esc` | Release the mouse cursor |

## Layout

```
src/            Engine sources
src/headers/    Headers, plus vendored stb_image.h and FastNoise.hpp
src/shaders/    GLSL vertex and fragment shaders
include/        Vendored GLM, GLFW, glad and KHR headers
lib/            Prebuilt GLFW for Windows/MinGW
content/        terrain.png texture atlas
```

## Licence

This project's own code is MIT licensed; see [LICENSE](LICENSE). It bundles
several third-party libraries under their own licences, listed in
[THIRD_PARTY.md](THIRD_PARTY.md).
