#include "headers/TerrainGen.hpp"

#include <cstdlib>

#include <algorithm>
#include <cmath>

#include "headers/ColumnCache.hpp"
#include "headers/Features.hpp"
#include "headers/WorldRandom.hpp"

namespace
{
    // The height field is sampled at 3.125 world units per block and the ore
    // style noises at 3, which is what the original generator used. They are
    // kept as named constants so the two scales stop looking like typos.
    const double HEIGHT_NOISE_SCALE = 3.125;
    const double MATERIAL_NOISE_SCALE = 3.0;

    // The relief was wide enough that most ground sat well above any sea level
    // low enough to keep the world from flooding, which left rivers cutting
    // channels through land they could never fill. A gentler range keeps the
    // bulk of the world within reach of the waterline.
    const int TERRAIN_BASE_HEIGHT = 45;
    const int TERRAIN_HEIGHT_RANGE = 20;

    // Rivers follow the line where the river noise crosses zero. Taking the
    // distance from that crossing gives a channel that runs along it, and
    // RIVER_EDGE is how far out from the line the channel reaches before the
    // ground returns to its normal height.
    const float RIVER_EDGE = 0.020f;

    // How far above sea level the ground can be before rivers stop. Climbing
    // ground narrows the channel rather than making it shallower: a channel
    // that is merely shallower stops reaching the water table, which is what
    // used to leave river-shaped valleys with no water in them, most obviously
    // across the higher ground of a desert.
    const float RIVER_MAX_CLIMB = 10.0f;

    // Where the temperature field starts turning to desert and where it is
    // fully desert. Reading it as a ramp rather than a threshold means the sand
    // thins out towards the edge of a desert instead of stopping at a line.
    const float DESERT_BEGINS = 0.06f;
    const float DESERT_FULL = 0.26f;

    // Same idea for woodland, which drives how thickly trees are placed.
    const float FOREST_BEGINS = 0.00f;
    const float FOREST_FULL = 0.30f;

    // Deep desert lays down this much sand over this much sandstone.
    const int DESERT_SAND_DEPTH = 5;
    const int DESERT_SANDSTONE_DEPTH = 4;

    // Riverbeds: gravel for the most part, with patches of sand still showing
    // through, clay in beds underneath it, and bare stone where the water is
    // deep enough that nothing settles.
    const int BED_SURFACE_DEPTH = 3;
    const int BED_CLAY_DEPTH = 3;
    const int BED_STONE_WATER_DEPTH = 7;
    const float BED_SAND_THRESHOLD = 0.10f;
    const float BED_CLAY_THRESHOLD = 0.18f;

    // Rivers are not one depth the whole way along. Long stretches run shallow
    // and every so often the channel drops into a pool, which is the only place
    // the water gets deep enough to scour its bed back to stone.
    const int RIVER_POOL_EXTRA_DEPTH = 5;
    const float POOL_BEGINS = 0.05f;
    const float POOL_FULL = 0.30f;

    // How far past the last visible block the material noise is still worked out.
    //
    // Zero is exact for what is drawn. Hashing every block the mesher would emit
    // a face for, over a hundred chunks, gives the same answer at zero as it
    // does with the noise run down the whole column -- only the buried blocks
    // differ, and nothing can see those. The margin exists because the world has
    // no digging yet: the moment a block can be broken, the block behind it
    // wants to be the gravel or dirt the noise would have put there rather than
    // flat stone.
    //
    // It is not free, measured with bench/ at ms per chunk:
    //
    //     margin 0    0.22        margin 4    0.71
    //     margin 2    0.44        margin 8    1.16
    //
    // so a margin big enough to matter for digging costs more than the whole
    // rest of generation. Kept at zero while nothing digs. When something does,
    // the answer is not to raise this -- it is to run the column through the
    // full fill again on the first break, which costs one column once instead of
    // every column always.
    const int MATERIAL_VISIBILITY_MARGIN = 0;

    float clampf(float value, float low, float high)
    {
        return std::min(std::max(value, low), high);
    }

    // Smooth ramp with zero slope at both ends, so the riverbank curves into the
    // surrounding ground instead of meeting it at a crease.
    float smoothstep(float t)
    {
        return t * t * (3.0f - 2.0f * t);
    }

    // Maps a raw noise reading onto 0..1 across a band, smoothly.
    float ramp(float value, float begins, float full)
    {
        return smoothstep(clampf((value - begins) / (full - begins), 0.0f, 1.0f));
    }

    // How much sand and sandstone a column gets for being arid. Shared by the
    // fill below and by SurfaceBlock, so that what decoration believes is on
    // top of a column cannot drift from what actually gets written there.
    int desertSandDepth(float aridity)
    {
        return static_cast<int>(aridity * DESERT_SAND_DEPTH + 0.5f);
    }

    int desertSandstoneDepth(float aridity)
    {
        return static_cast<int>(aridity * DESERT_SANDSTONE_DEPTH + 0.5f);
    }
}


TerrainGen::TerrainGen(uint64_t seed)
{
    _seed = seed;

    // Six octaves, not sixteen. Lacunarity doubles the frequency each time, so
    // from a base of 0.00153 the sixteenth octave lands near 50 -- far past one
    // cycle per block, which is the most a per-block sample can carry. Those
    // octaves were sampling noise finer than the world can hold and folding it
    // back in as a contribution of under a thousandth of a block of height.
    _heightNoise.SetSeed(static_cast<int>(seed));
    _heightNoise.SetNoiseType(FastNoise::PerlinFractal);
    _heightNoise.SetFrequency(0.00153f);
    _heightNoise.SetFractalOctaves(6);

    _gravelNoise.SetSeed(static_cast<int>(seed));
    // The fractal settings below do nothing: FastNoise only applies octaves,
    // lacunarity and gain to the *Fractal noise types, and Cellular is not one.
    // They are left in place because removing them would change nothing, and
    // noted here so the next reader does not tune numbers that are never read.
    _gravelNoise.SetNoiseType(FastNoise::Cellular);
    _gravelNoise.SetFrequency(0.03f);
    _gravelNoise.SetFractalOctaves(6);
    _gravelNoise.SetFractalLacunarity(1.86f);
    _gravelNoise.SetFractalGain(3.0f);
    _gravelNoise.SetCellularReturnType(FastNoise::Distance2Add);

    _dirtNoise.SetSeed(static_cast<int>(seed + 1));
    _dirtNoise.SetNoiseType(FastNoise::Cellular);
    _dirtNoise.SetFrequency(0.03f);
    _dirtNoise.SetFractalOctaves(6);
    _dirtNoise.SetFractalLacunarity(1.86f);
    _dirtNoise.SetFractalGain(3.0f);
    _dirtNoise.SetCellularReturnType(FastNoise::Distance2Add);

    // A couple of octaves so the channels wander instead of running in smooth
    // arcs, but not so many that they fray into disconnected ponds.
    _riverNoise.SetSeed(static_cast<int>(seed + 2));
    _riverNoise.SetNoiseType(FastNoise::PerlinFractal);
    _riverNoise.SetFrequency(0.0022f);
    _riverNoise.SetFractalOctaves(3);

    // Very low frequency, so a desert is something you walk into over a few
    // hundred blocks rather than something that changes every chunk.
    _temperatureNoise.SetSeed(static_cast<int>(seed + 3));
    _temperatureNoise.SetNoiseType(FastNoise::PerlinFractal);
    _temperatureNoise.SetFrequency(0.0007f);
    _temperatureNoise.SetFractalOctaves(3);

    // Higher frequency than temperature: woodland gathers into stands with
    // clearings between them, inside a biome rather than being one.
    _forestNoise.SetSeed(static_cast<int>(seed + 4));
    _forestNoise.SetNoiseType(FastNoise::PerlinFractal);
    _forestNoise.SetFrequency(0.0035f);
    _forestNoise.SetFractalOctaves(2);

    // Both riverbed fields run at a high enough frequency to give patches a few
    // blocks across, so a bed is mottled rather than uniform.
    _bedNoise.SetSeed(static_cast<int>(seed + 5));
    _bedNoise.SetNoiseType(FastNoise::Perlin);
    _bedNoise.SetFrequency(0.060f);

    _clayNoise.SetSeed(static_cast<int>(seed + 6));
    _clayNoise.SetNoiseType(FastNoise::Perlin);
    _clayNoise.SetFrequency(0.045f);

    // Low enough frequency that a pool is a stretch of river rather than a
    // pothole in it.
    _poolNoise.SetSeed(static_cast<int>(seed + 7));
    _poolNoise.SetNoiseType(FastNoise::Perlin);
    _poolNoise.SetFrequency(0.006f);
}


void TerrainGen::GenerateChunk(BlockMap& map, int chunkX, int chunkZ) const
{
    // One pass over the noise for the whole chunk and its margin. The fill needs
    // the margin too, because how deep a column can be seen depends on the
    // columns beside it, and at a chunk edge those are in the next chunk over.
    ColumnCache cache;

    // Heights first, and wider, because the beach test reads the columns around
    // each one and those run past the margin the rest of the cache needs.
    for (int x = 0; x < HEIGHT_WIDTH; x++)
    {
        for (int z = 0; z < HEIGHT_WIDTH; z++)
        {
            int worldX = chunkX * CHUNK_WIDTH + x - DECORATION_MARGIN - terrain::BEACH_RADIUS;
            int worldZ = chunkZ * CHUNK_WIDTH + z - DECORATION_MARGIN - terrain::BEACH_RADIUS;
            cache.heights[x][z] = SurfaceYAt(worldX, worldZ);
        }
    }

    for (int x = 0; x < PADDED_WIDTH; x++)
    {
        for (int z = 0; z < PADDED_WIDTH; z++)
        {
            int worldX = chunkX * CHUNK_WIDTH + x - DECORATION_MARGIN;
            int worldZ = chunkZ * CHUNK_WIDTH + z - DECORATION_MARGIN;

            ColumnInfo& column = cache.columns[x][z];
            column = ColumnAt(worldX, worldZ);

            // Dry, low, and actually beside water. The height test comes first
            // because it is two comparisons and rules out most of the map, which
            // keeps the neighbour walk off all of it.
            const int localX = x - DECORATION_MARGIN;
            const int localZ = z - DECORATION_MARGIN;

            column.beach = column.surfaceY <= terrain::SHORE_HEIGHT &&
                           !column.underwater() &&
                           cache.NearWater(localX, localZ);
        }
    }

    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        for (int z = 0; z < CHUNK_WIDTH; z++)
        {
            int worldX = chunkX * CHUNK_WIDTH + x;
            int worldZ = chunkZ * CHUNK_WIDTH + z;

            const ColumnInfo& column = cache.At(x, z);

            // A block sits in daylight only if it is higher than the ground in
            // at least one of the four directions, so the deepest block that
            // can ever be seen from the side is one above the lowest of them.
            int visibleDepth = std::max(0, column.surfaceY - cache.LowestNeighbour(x, z) - 1)
                               + MATERIAL_VISIBILITY_MARGIN;

            FillColumn(map, x, z, worldX, worldZ, column, visibleDepth);
        }
    }

    // Terrain has to be complete before anything is stood on top of it.
    DecorateChunk(map, cache, *this, _seed, chunkX, chunkZ);
}


float TerrainGen::SurfaceHeightAt(int worldX, int worldZ, float* riverStrength) const
{
    float base = _heightNoise.GetNoise(worldX * HEIGHT_NOISE_SCALE, worldZ * HEIGHT_NOISE_SCALE)
                 * TERRAIN_HEIGHT_RANGE + TERRAIN_BASE_HEIGHT;

    // Distance from the line where the river noise crosses zero. That line is
    // continuous and wanders, which is what makes the channel read as a river
    // rather than as a chain of pools.
    float distanceFromChannel = std::fabs(_riverNoise.GetNoise(worldX, worldZ));

    // Climbing ground narrows the channel and eventually closes it, rather than
    // leaving a wide one that no longer cuts deep enough to hold water.
    float edge = RIVER_EDGE * clampf(1.0f - (base - terrain::SEA_LEVEL) / RIVER_MAX_CLIMB, 0.0f, 1.0f);

    float strength = 0.0f;
    if (edge > 0.0f && distanceFromChannel < edge)
        strength = smoothstep(1.0f - distanceFromChannel / edge);

    float height = base;

    // The pool noise is only asked for inside a channel. Outside one the bed
    // depth it produces is multiplied by a strength of zero and thrown away, and
    // the beach test below samples a great many columns that are nowhere near a
    // river -- so this is the difference between two noise lookups and three
    // across most of the map.
    if (strength > 0.0f)
    {
        // Because the centre of a channel always carries full strength, it
        // always cuts to the bed, and so any river that exists at all has water
        // in it.
        int bedDepth = terrain::RIVER_BED_DEPTH +
                       static_cast<int>(ramp(_poolNoise.GetNoise(worldX, worldZ), POOL_BEGINS, POOL_FULL)
                                        * RIVER_POOL_EXTRA_DEPTH + 0.5f);

        float bed = static_cast<float>(terrain::SEA_LEVEL - bedDepth);
        if (bed < base)
            height = base + (bed - base) * strength;
    }

    if (riverStrength)
        *riverStrength = strength;

    return height;
}

int TerrainGen::SurfaceYAt(int worldX, int worldZ) const
{
    // Leave room for bedrock underneath and for anything the decorator stacks
    // on top, so neither has to bounds check against the ends of the column.
    float height = SurfaceHeightAt(worldX, worldZ, NULL);
    return std::min(std::max(static_cast<int>(height), 1), CHUNK_HEIGHT - 16);
}

ColumnInfo TerrainGen::ColumnAt(int worldX, int worldZ) const
{
    ColumnInfo column;

    float strength = 0.0f;
    float height = SurfaceHeightAt(worldX, worldZ, &strength);

    column.riverStrength = strength;
    column.surfaceY = std::min(std::max(static_cast<int>(height), 1), CHUNK_HEIGHT - 16);

    // beach is left false here and filled in by GenerateChunk, which has the
    // heights of the surrounding columns to hand. It cannot be decided from this
    // column alone.

    column.aridity = ramp(_temperatureNoise.GetNoise(worldX, worldZ), DESERT_BEGINS, DESERT_FULL);
    column.forest = ramp(_forestNoise.GetNoise(worldX, worldZ), FOREST_BEGINS, FOREST_FULL);

    // A river keeps its own banks green, so a channel crossing a desert reads
    // as an oasis rather than as a strip of sand in more sand.
    column.aridity *= 1.0f - strength;

    if (column.aridity > 0.5f)
        column.biome = DESERT;
    else if (column.forest > 0.5f)
        column.biome = FOREST;
    else
        column.biome = PLAINS;

    return column;
}


// What ends up on top of a column. Decoration needs this for columns outside
// the chunk it is building, where there is no block map to read: a tree rooted
// two blocks into the next chunk still has to be accepted or rejected the same
// way from both sides.
unsigned short TerrainGen::SurfaceBlock(int worldX, int worldZ, const ColumnInfo& column) const
{
    if (column.underwater())
    {
        if (terrain::SEA_LEVEL - column.surfaceY >= BED_STONE_WATER_DEPTH)
            return STONE;

        return (_bedNoise.GetNoise(worldX, worldZ) > BED_SAND_THRESHOLD) ? SAND : GRAVEL;
    }

    if (column.shore())
        return SAND;

    if (desertSandDepth(column.aridity) > 0)
        return SAND;

    return GRASS;
}


void TerrainGen::FillColumn(BlockMap& map, int localX, int localZ,
                            int worldX, int worldZ, const ColumnInfo& column,
                            int visibleDepth) const
{
    // How deep the dirt runs under the grass. This was a rand() call made once
    // per block, which meant it varied within a single column and differed run
    // to run; hashing the column gives one stable depth per column instead.
    WorldRandom depthRandom(_seed, worldX, worldZ, salt::DIRT_DEPTH);
    int softDepth = depthRandom.nextInt(3, 4);

    // What the top of the column is made of. Dry ground at or just above the
    // waterline is beach; dry ground goes to sand over sandstone as it turns to
    // desert, and to grass over dirt otherwise. Both depths ramp with aridity,
    // so the edge of a desert thins out rather than ending on a line.
    int sandDepth = 0;
    int sandstoneDepth = 0;

    bool riverbed = column.underwater();

    if (!riverbed)
    {
        if (column.shore())
        {
            sandDepth = softDepth + 1;
        }
        else
        {
            sandDepth = desertSandDepth(column.aridity);
            sandstoneDepth = desertSandstoneDepth(column.aridity);
        }
    }

    // Anything under water gets a riverbed instead: mostly gravel, with sand
    // patches still coming through, clay laid under it in beds, and stone left
    // bare where the water is deep enough that nothing settles on it.
    bool sandyBed = false;
    bool clayBed = false;
    bool stoneBed = false;

    if (riverbed)
    {
        sandyBed = _bedNoise.GetNoise(worldX, worldZ) > BED_SAND_THRESHOLD;
        clayBed = _clayNoise.GetNoise(worldX, worldZ) > BED_CLAY_THRESHOLD;
        stoneBed = (terrain::SEA_LEVEL - column.surfaceY) >= BED_STONE_WATER_DEPTH;
    }

    double materialX = worldX * MATERIAL_NOISE_SCALE;
    double materialZ = worldZ * MATERIAL_NOISE_SCALE;

    // Everything below this is buried on all four sides and can only ever be
    // stone, so the two cellular lookups below are skipped for it. They are the
    // whole cost of generating a chunk -- a little over five milliseconds of the
    // five and a half -- and the mean column only shows about one block of its
    // depth, so nearly all of that work was for blocks nobody can see.
    int deepestVisibleY = column.surfaceY - visibleDepth;

    for (int y = 0; y <= column.surfaceY; y++)
    {
        int depth = column.surfaceY - y;
        unsigned short block = STONE;

        if (y >= deepestVisibleY)
        {
            if (_gravelNoise.GetNoise(materialX, y * MATERIAL_NOISE_SCALE, materialZ) < 0.3)
                block = GRAVEL;

            if (_dirtNoise.GetNoise(materialX, y * MATERIAL_NOISE_SCALE, materialZ) < 0.3)
                block = DIRT;
        }

        if (y == 0)
        {
            block = BEDROCK;
        }
        else if (riverbed)
        {
            if (depth < BED_SURFACE_DEPTH)
                block = stoneBed ? STONE : (sandyBed ? SAND : GRAVEL);
            else if (clayBed && depth < BED_SURFACE_DEPTH + BED_CLAY_DEPTH)
                block = CLAY;
        }
        else if (depth < sandDepth)
        {
            block = SAND;
        }
        else if (depth < sandDepth + sandstoneDepth)
        {
            block = SANDSTONE;
        }
        else if (sandDepth == 0 && depth <= softDepth)
        {
            block = (depth == 0) ? GRASS : DIRT;
        }

        map.Set(localX, y, localZ, block);
    }

    for (int y = column.surfaceY + 1; y <= terrain::SEA_LEVEL; y++)
    {
        map.Set(localX, y, localZ, WATER);
    }

    // No air fill above the column. A BlockMap starts out all air, every map is
    // built once and never reused, and AIR is zero, so writing it back over the
    // sixty-odd empty blocks at the top of each of the 256 columns was fifteen
    // thousand stores a chunk to leave memory exactly as it was found.
}
