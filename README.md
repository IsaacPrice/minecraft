# minecraft

A Minecraft-style voxel renderer written from scratch in C++ with OpenGL 3.3.
It generates an infinite-feeling world from Perlin and cellular noise, meshes it
into chunks, and lets you fly around it.

## Features

- Procedural terrain from a seeded noise stack (height, gravel, dirt,
  rivers, temperature, woodland)
- Rivers carved along a wandering noise channel, with lakes in low ground,
  sand banks along every shore, and beds of gravel, sand, clay and stone
- Desert biome, blended at its edges, laying sand over sandstone
- Trees, tall grass, roses and dandelions, mushrooms, cacti and dead
  shrubs on desert sand, sugar cane in stands along the waterline, and
  pumpkin patches
- 16 x 255 x 16 chunks, meshed with interior and shared faces culled
- Chunk streaming: chunks load and unload as you move
- Multithreaded chunk generation across the available cores
- Texture atlas sampling for per-block, per-face textures, with alpha
  cut-out plants and a blended pass for water
- Free-fly camera with mouse look

Generation is a pure function of the seed and the world coordinate, so the
same seed always gives the same world, and a feature that straddles a chunk
border comes out whole no matter which of the two chunks is built first.

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

The seed is printed to the console at startup. Passing it back is not wired
up to the command line yet, but setting it in `main.cpp` reproduces a world
exactly.

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
src/TerrainGen  Height, rivers, biomes: what a column of world is made of
src/Features    Trees and plants placed on top of finished terrain
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
