// Headless timing harness for the parts of the engine that need no GL context.
//
// Terrain generation and meshing are pure CPU work on data structures that know
// nothing about OpenGL, so they can be timed without opening a window. That
// makes it possible to measure a generation change in isolation, instead of
// trying to read it off a frame rate that vsync has already flattened.
//
//   mingw32-make bench && ./bench
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

#include "../src/headers/Chunk.hpp"
#include "../src/headers/TerrainGen.hpp"

namespace
{
    typedef std::chrono::high_resolution_clock Clock;

    double millisBetween(Clock::time_point start, Clock::time_point end)
    {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    // The square of chunks each stage is timed over. Meshing needs the four
    // chunks around the one being meshed, so the outermost ring is generated but
    // never meshed, and the timings below divide by the right count for each.
    const int GRID = 8;
    const int MESHED = (GRID - 2) * (GRID - 2);
}


int main()
{
    TerrainGen terrain(12345);

    std::vector<BlockMapPtr> maps;
    maps.reserve(GRID * GRID);

    Clock::time_point generateStart = Clock::now();
    for (int index = 0; index < GRID * GRID; index++)
    {
        BlockMapPtr map = std::make_shared<BlockMap>();
        terrain.GenerateChunk(*map, index % GRID, index / GRID);
        maps.push_back(map);
    }
    double generateMs = millisBetween(generateStart, Clock::now());

    Clock::time_point meshStart = Clock::now();
    size_t vertices = 0;
    for (int chunkZ = 1; chunkZ < GRID - 1; chunkZ++)
    {
        for (int chunkX = 1; chunkX < GRID - 1; chunkX++)
        {
            Chunk chunk;
            chunk.chunkPos = { static_cast<float>(chunkX), static_cast<float>(chunkZ) };
            chunk.blocks = maps[chunkX + chunkZ * GRID];
            chunk.MakeVertexObject(*maps[(chunkX - 1) + chunkZ * GRID],
                                   *maps[(chunkX + 1) + chunkZ * GRID],
                                   *maps[chunkX + (chunkZ - 1) * GRID],
                                   *maps[chunkX + (chunkZ + 1) * GRID]);
            vertices += chunk.SolidVertexCount() + chunk.WaterVertexCount();
        }
    }
    double meshMs = millisBetween(meshStart, Clock::now());

    double generatePerChunk = generateMs / (GRID * GRID);
    double meshPerChunk = meshMs / MESHED;
    size_t verticesPerChunk = vertices / MESHED;

    printf("GenerateChunk    %7.3f ms/chunk\n", generatePerChunk);
    printf("MakeVertexObject %7.3f ms/chunk\n", meshPerChunk);
    printf("vertices         %7zu /chunk\n", verticesPerChunk);
    printf("sizeof(BlockMap) %7zu bytes\n", sizeof(BlockMap));

    // What those per-chunk costs come to over a full 65x65 render distance.
    const int LOADED = 65 * 65;
    printf("\nover %d chunks (render distance 64):\n", LOADED);
    printf("  generate  %6.2f s of CPU work\n", generatePerChunk * LOADED / 1000.0);
    printf("  mesh      %6.2f s of CPU work\n", meshPerChunk * LOADED / 1000.0);
    printf("  block maps  %4.0f MB if all resident\n", LOADED * sizeof(BlockMap) / 1048576.0);

    return 0;
}
