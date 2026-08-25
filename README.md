# minecraft

A Minecraft-style voxel renderer written from scratch in C++ with OpenGL 3.3.
It generates an infinite-feeling world from Perlin and cellular noise, meshes it
into chunks, and lets you fly around it.

## Features

- Procedural terrain from a seeded noise stack (height, gravel, dirt,
  rivers, temperature, woodland)
- Rivers carved along a wandering noise channel, with lakes in low ground,
  a band of sand along every shore, and beds of gravel, sand, clay and stone
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
- Pause and options menus drawn in-engine, over a bitmap font: rebindable
  keys, mouse sensitivity, field of view, and graphics settings from render
  distance to window mode. Saved to `options.txt` and reloaded at startup

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

The seed is printed to the console at startup. Passing it back is not wired
up to the command line yet, but setting it in `main.cpp` reproduces a world
exactly.

## Controls

Every key below except `Esc` can be rebound from Options -> Controls.

| Key | Action |
| --- | --- |
| `W` `A` `S` `D` | Move horizontally |
| `Space` | Fly up |
| `Left Shift` | Fly down |
| Mouse | Look around |
| `F3` | Debug overlay |
| `Esc` | Pause menu, and back out of it one screen at a time |

## Debug overlay

`F3` puts a performance readout over the world without pausing it:

- Frame rate, average frame time, and the worst frame in the last half second --
  a stutter is what gets noticed, and an average hides it completely
- Whether frames are waiting on the display or on the frame cap
- Chunks drawn out of chunks loaded, and how many are still queued to build
- Triangles submitted this frame
- How much terrain is cached, in chunks and in megabytes. At a large render
  distance this is by far the biggest thing the process holds
- Position in blocks, the chunk it falls in, and which way the camera faces
- Render distance, graphics style, antialiasing, and the seed

It is a HUD rather than a screen: it takes no input beyond its own toggle, does
not stack, and the game keeps running underneath. It shares one vertex batch with
the menus, so the whole interface is a single draw call however much is showing.

## Settings

`Esc` opens the pause menu. Under Settings:

| Setting | |
| --- | --- |
| Field of View | 30 to 110 degrees |
| Sensitivity | 10% to 300% of the base look speed |
| Invert Mouse Y | |
| Controls... | Click a key to rebind it, any key or mouse button; `Esc` cancels. Taking a key from another action leaves that one unbound and shown in red |
| Graphics... | below |

And under Graphics:

| Setting | |
| --- | --- |
| Render Distance | 4 to 96 chunks, as a radius from the player |
| Foliage Distance | Past this, small plants are dropped and canopies go solid |
| Graphics | Fancy meshes every face of every leaf block, at every depth through a canopy, so the gaps in the leaf tile have more leaves behind them. Fast treats a leaf as an ordinary opaque block. Changing this remeshes the world, which takes a moment |
| Antialiasing | The FXAA resolve pass |
| VSync | |
| Anisotropic Filtering | Off to 16x, capped at what the driver reports |
| Max Framerate | 30 to 240 fps in tens, then unlimited |
| Window Mode | Windowed, Borderless or Fullscreen |
| Resolution | The sizes the monitor reports. Borderless ignores it and takes the monitor's own |

Field of view, sensitivity, the graphics style, antialiasing, vsync,
anisotropic filtering and the frame cap all apply as they are changed. Render
distance, window mode and resolution apply when the Graphics screen is left,
because each one tears something down and rebuilds it -- the chunk worker pool,
or the window and the offscreen buffer behind it -- and doing that on every
frame of a drag would be unusable.

Settings are written to `options.txt` in the working directory when the menu is
closed and when the game exits. Delete it to go back to defaults; an unknown or
out-of-range entry is ignored or clamped rather than refused, so a file from an
older build still loads.

Render distance is a **radius**, so 32 means 32 chunks in every direction. The
value handed to `World` is twice that, because `World` counts the width of the
loaded square instead; `worldRenderDistance` in `main.cpp` is the one place that
conversion happens.

## Layout

```
src/            Engine sources
src/TerrainGen  Height, rivers, biomes: what a column of world is made of
src/Features    Trees and plants placed on top of finished terrain
src/Display     The window: mode, resolution, vsync and the frame cap
src/Input       Actions and bindings, and the GLFW input callbacks
src/Settings    Every value the options menu can change, and options.txt
src/UIRenderer  2D quad batch and bitmap font, for anything drawn on the screen
src/Widgets     Button, slider, toggle
src/Screens     The screen stack and the four pages of menu
src/headers/    Headers, plus vendored stb_image.h and FastNoise.hpp
src/shaders/    GLSL vertex and fragment shaders
bench/          Headless timing harness for generation and meshing
include/        Vendored GLM, GLFW, glad and KHR headers
lib/            Prebuilt GLFW for Windows/MinGW
content/        terrain.png texture atlas, and font.png for the menus
content/scripts make_font.py, which bakes font.png
```

## Licence

This project's own code is MIT licensed; see [LICENSE](LICENSE). It bundles
several third-party libraries under their own licences, listed in
[THIRD_PARTY.md](THIRD_PARTY.md).
