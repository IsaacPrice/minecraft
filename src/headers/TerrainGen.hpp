#pragma once

#include <cstdint>

#include "BlockMap.hpp"
#include "FastNoise.hpp"

namespace terrain
{
    // Everything below this fills with water where the ground does not reach it.
    // The height field centres on 45, so this sits low enough that ordinary
    // ground stays dry and only carved channels and natural hollows flood.
    const int SEA_LEVEL = 41;

    // How far under sea level a river cuts at its centre, before the pools
    // along it deepen that further.
    const int RIVER_BED_DEPTH = 4;

    // Sand is laid on dry ground at or below this height -- but only where
    // there is actually water within BEACH_RADIUS blocks. Height alone used to
    // be the whole test, which is why any low ground was a beach whether or not
    // it had ever seen water: the height field centres on 45 with a range of 20,
    // so a great deal of ordinary flat plain sits at 41 or 42 and came out as
    // desert-sized sand flats with no lake anywhere near them.
    const int SHORE_HEIGHT = SEA_LEVEL + 1;

    // How far from water sand reaches, in blocks. Small on purpose: a beach is
    // the edge of a body of water, not the ground around it.
    const int BEACH_RADIUS = 3;
}


enum BIOME
{
    PLAINS,
    FOREST,
    DESERT
};


// Everything the generator knows about one column of the world, worked out from
// noise alone. Nothing in here depends on which chunk is asking, so a chunk can
// ask about columns outside itself -- which is how features that straddle a
// chunk border get placed consistently from both sides.
struct ColumnInfo
{
    int surfaceY = 0;         // topmost solid block
    BIOME biome = PLAINS;
    float riverStrength = 0;  // 0 outside a river, up to 1 at its centre

    // Kept as continuous values rather than being folded into the biome, so a
    // biome edge can be blended instead of drawn as a line, and so decoration
    // can thin out towards that edge instead of stopping dead at it.
    float aridity = 0;        // 0 lush, 1 deep desert
    float forest = 0;         // 0 open ground, 1 dense woodland

    // Worked out in ColumnAt rather than derived here, because it depends on
    // the columns around this one and not only on this one's height.
    bool beach = false;

    bool underwater() const { return surfaceY < terrain::SEA_LEVEL; }
    bool shore() const { return beach; }
};


class TerrainGen
{
public:
    TerrainGen() = default;
    explicit TerrainGen(uint64_t seed);

    // Fills one chunk of block ids. This is the entry point the workers use.
    void GenerateChunk(BlockMap& map, int chunkX, int chunkZ) const;

    // Pure functions of world coordinates. Both are const and touch no shared
    // state, so any number of workers can call them at once.
    ColumnInfo ColumnAt(int worldX, int worldZ) const;
    unsigned short SurfaceBlock(int worldX, int worldZ, const ColumnInfo& column) const;
    // visibleDepth is how far below the surface this column can still be seen
    // from, worked out from how tall the columns around it are. Below that the
    // fill lays plain stone instead of asking the material noise, which is
    // where nearly all of the generator's time used to go.
    void FillColumn(BlockMap& map, int localX, int localZ,
                    int worldX, int worldZ, const ColumnInfo& column,
                    int visibleDepth) const;

    uint64_t Seed() const { return _seed; }

private:
    // The height field and the river carve, which between them are the whole of
    // what decides whether a column ends up under water. Split out of ColumnAt
    // because the beach test has to ask the same question of the columns around
    // this one, and does not need the biome and decoration noise ColumnAt also
    // gathers. riverStrength may be null where the caller only wants the height.
    float SurfaceHeightAt(int worldX, int worldZ, float* riverStrength) const;

public:
    // Public because ColumnCache fills its height grid with it. Pure in world
    // coordinates, like everything else here, so two chunks meeting at a border
    // agree about where the sand stops.
    int SurfaceYAt(int worldX, int worldZ) const;

private:

    uint64_t _seed = 0;

    // Shared across every worker rather than copied per worker. FastNoise fills
    // its permutation tables in SetSeed and only reads them afterwards, and all
    // of its sampling entry points are const, so concurrent reads are safe.
    FastNoise _heightNoise;
    FastNoise _gravelNoise;
    FastNoise _dirtNoise;
    FastNoise _riverNoise;
    FastNoise _temperatureNoise;
    FastNoise _forestNoise;
    FastNoise _bedNoise;
    FastNoise _clayNoise;
    FastNoise _poolNoise;
};
