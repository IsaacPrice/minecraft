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
- 16 x 128 x 16 chunks, meshed with interior and shared faces culled
- Chunk streaming: chunks load and unload as you move, nearest first, on a
  per-frame upload budget
- Multithreaded chunk generation across the available cores
- Frustum culling and front-to-back drawing, over a packed twelve byte vertex
- Distance fog, so the world fades into the sky where the chunks run out
- Texture atlas sampling for per-block, per-face textures, with alpha
  cut-out plants and a blended pass for water
- Free-fly camera with mouse look

Generation is a pure function of the seed and the world coordinate, so the
same seed always gives the same world, and a feature that straddles a chunk
border comes out whole no matter which of the two chunks is built first.

Only the part of a column that can actually be seen is worked out in full.
A block buried on all four sides can only ever be stone, so the material
noise is not run for it -- which is most of the column, and was most of the
time generation used to take.

Only the part of a column that can actually be seen is worked out in full. The
material noise is the whole cost of generating a chunk, and the average column
shows about one block of its depth, so everything sealed in on all four sides is
laid as plain stone. Hashing every block the mesher would draw a face for gives
the same answer either way; see `MATERIAL_VISIBILITY_MARGIN` in `TerrainGen.cpp`
for what that trades away and when it should change.

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

### Benchmarking

Generation and meshing are plain CPU work on data structures that know nothing
about OpenGL, so they can be timed without opening a window:

```bash
make bench && ./benchmark
```

Useful for telling an optimisation from a placebo: a frame rate is capped by
vsync and moves with whatever else the machine is doing, while these numbers
are the work itself.

## Running

Run from the project root, so the shader and texture paths resolve:

```bash
./app
```

Pass `--novsync` to unpin the frame rate from the refresh rate. The window title
carries the frame time, the visible and loaded chunk counts and the triangle
count, and the same line goes to stdout once a second so two runs can be
compared.

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
| `V` | Toggle vsync |
| `Esc` | Release the mouse cursor |

## Benchmarking

Generation and meshing need no OpenGL context, so they can be timed on their own
rather than read off a frame rate that vsync has already flattened:

```bash
make bench && ./benchmark
```

## Layout

```
src/            Engine sources
src/TerrainGen  Height, rivers, biomes: what a column of world is made of
src/Features    Trees and plants placed on top of finished terrain
src/headers/    Headers, plus vendored stb_image.h and FastNoise.hpp
src/shaders/    GLSL vertex and fragment shaders
bench/          Headless timing harness for generation and meshing
include/        Vendored GLM, GLFW, glad and KHR headers
lib/            Prebuilt GLFW for Windows/MinGW
content/        terrain.png texture atlas
```

## Licence

This project's own code is MIT licensed; see [LICENSE](LICENSE). It bundles
several third-party libraries under their own licences, listed in
[THIRD_PARTY.md](THIRD_PARTY.md).
