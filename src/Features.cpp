#include "headers/Features.hpp"

#include <algorithm>

#include "headers/WorldRandom.hpp"

namespace
{
    // Trees are placed one per cell of a grid rather than by rolling a dice per
    // column. Rolling per column clumps trunks together and lets canopies grow
    // through each other; a grid gives every tree a guaranteed patch of its own,
    // and jittering its position inside that cell keeps the result from looking
    // like an orchard.
    const int TREE_GRID = 5;

    const int TREE_MIN_HEIGHT = 4;
    const int TREE_MAX_HEIGHT = 6;

    // Pumpkins grow in patches rather than singly, so a coarse grid decides
    // which areas are patches at all and only those roll for pumpkins.
    const int PUMPKIN_PATCH_GRID = 32;
    const float PUMPKIN_PATCH_CHANCE = 0.030f;
    const float PUMPKIN_IN_PATCH_CHANCE = 0.070f;

    // Flowers grow in drifts of one kind, so which kind is decided per area.
    const int FLOWER_PATCH_GRID = 24;

    // Sugar cane grows in stands along a bank rather than lining the whole of
    // it. A coarse grid picks which stretches of waterline carry a stand, and
    // only those roll per column, which leaves gaps between the clumps.
    const int SUGAR_CANE_PATCH_GRID = 13;
    const float SUGAR_CANE_PATCH_CHANCE = 0.14f;

    const float TALL_GRASS_CHANCE = 0.140f;
    const float FLOWER_CHANCE = 0.006f;
    const float MUSHROOM_CHANCE = 0.008f;
    const float DEAD_SHRUB_CHANCE = 0.006f;
    const float SUGAR_CANE_CHANCE = 0.90f;
    const float CACTUS_CHANCE = 0.006f;

    const int CACTUS_MIN_HEIGHT = 1;
    const int CACTUS_MAX_HEIGHT = 3;

    // Rounds towards negative infinity, which integer division does not do. Grid
    // cells west or north of the origin would otherwise be twice the width of
    // the others and every feature in them would shift by one cell.
    int floorDiv(int value, int divisor)
    {
        return (value >= 0) ? (value / divisor)
                            : -(((-value) + divisor - 1) / divisor);
    }

    const int PADDED_WIDTH = CHUNK_WIDTH + 2 * DECORATION_MARGIN;

    // The terrain of every column this chunk might need, including the margin
    // outside it. Worked out once and shared by all the features, because
    // several of them ask about the same columns and about their neighbours.
    struct ColumnCache
    {
        ColumnInfo columns[PADDED_WIDTH][PADDED_WIDTH];

        // Indexed in chunk-local coordinates, which run from -DECORATION_MARGIN
        // to CHUNK_WIDTH + DECORATION_MARGIN - 1.
        const ColumnInfo& At(int localX, int localZ) const
        {
            int x = std::min(std::max(localX + DECORATION_MARGIN, 0), PADDED_WIDTH - 1);
            int z = std::min(std::max(localZ + DECORATION_MARGIN, 0), PADDED_WIDTH - 1);
            return columns[x][z];
        }
    };

    // Writes a block only onto air, so decoration never eats the terrain it
    // stands on, never carves into water, and never overwrites another feature.
    void placeOnAir(BlockMap& map, int localX, int y, int localZ, unsigned short block)
    {
        if (localX < 0 || localX >= CHUNK_WIDTH || localZ < 0 || localZ >= CHUNK_WIDTH)
            return;
        if (y < 0 || y >= CHUNK_HEIGHT)
            return;
        if (map.Get(localX, y, localZ) != AIR)
            return;

        map.Set(localX, y, localZ, block);
    }

    bool insideChunk(int local)
    {
        return local >= 0 && local < CHUNK_WIDTH;
    }


    // --- trees --------------------------------------------------------------

    // Builds the tree rooted in one grid cell, if there is one. Every block of
    // it is offered to the chunk and the ones outside are dropped, so the shape
    // does not depend on which chunk is asking.
    void placeTree(BlockMap& map, const ColumnCache& cache, const TerrainGen& terrain,
                   uint64_t seed, int chunkX, int chunkZ, int gridX, int gridZ)
    {
        WorldRandom random(seed, gridX, gridZ, salt::TREE);

        // Drawn before anything can return, so the stream stays in step no
        // matter which chunk is running this.
        int worldX = gridX * TREE_GRID + random.nextInt(0, TREE_GRID - 1);
        int worldZ = gridZ * TREE_GRID + random.nextInt(0, TREE_GRID - 1);
        float roll = random.nextFloat();
        int height = random.nextInt(TREE_MIN_HEIGHT, TREE_MAX_HEIGHT);

        int localX = worldX - chunkX * CHUNK_WIDTH;
        int localZ = worldZ - chunkZ * CHUNK_WIDTH;

        // The grid is scanned by cell, and a cell can put its trunk further out
        // than the margin, in which case no part of the tree reaches this chunk.
        // Dropping those here keeps every lookup below inside the cache instead
        // of leaning on it to clamp. It is safe to return after the draws above,
        // because which trees exist does not depend on who is asking.
        if (localX < -DECORATION_MARGIN || localX >= CHUNK_WIDTH + DECORATION_MARGIN ||
            localZ < -DECORATION_MARGIN || localZ >= CHUNK_WIDTH + DECORATION_MARGIN)
        {
            return;
        }

        const ColumnInfo& column = cache.At(localX, localZ);

        // Woodland carries most of the trees, but open plains keep a few, so
        // the line between the two does not read as a wall of forest.
        float density = 0.02f + column.forest * column.forest * 0.60f;

        if (roll >= density)
            return;

        // Asked of the terrain rather than of the block map, because the trunk
        // column is often outside this chunk and has no blocks here to read.
        // Both chunks that can see this tree ask the same question this way and
        // get the same answer, which is what keeps the two halves in agreement.
        if (terrain.SurfaceBlock(worldX, worldZ, column) != GRASS)
            return;

        int trunkTop = column.surfaceY + height;

        // Canopy first, so the trunk overwrites any leaf that lands on it.
        // Iterating the whole canopy rather than only the part inside this
        // chunk keeps the random draws below in the same order everywhere.
        for (int y = trunkTop - 2; y <= trunkTop + 1; y++)
        {
            int radius = (y <= trunkTop - 1) ? 2 : 1;

            for (int dx = -radius; dx <= radius; dx++)
            {
                for (int dz = -radius; dz <= radius; dz++)
                {
                    bool corner = (dx == -radius || dx == radius) &&
                                  (dz == -radius || dz == radius);

                    // The corners of the widest layers are thinned out at
                    // random, which is what stops the canopy reading as a box.
                    bool trimmed = corner && (y >= trunkTop || random.chance(0.5f));

                    if (trimmed)
                        continue;

                    placeOnAir(map, localX + dx, y, localZ + dz, LEAVES);
                }
            }
        }

        // The trunk goes in after the canopy so it overwrites any leaf that
        // landed on it, and with Set rather than placeOnAir for the same reason.
        for (int y = column.surfaceY + 1; y <= trunkTop; y++)
        {
            if (insideChunk(localX) && insideChunk(localZ) && y < CHUNK_HEIGHT)
                map.Set(localX, y, localZ, OAK_LOG);
        }
    }

    void placeTrees(BlockMap& map, const ColumnCache& cache, const TerrainGen& terrain,
                    uint64_t seed, int chunkX, int chunkZ)
    {
        // Every grid cell holding a trunk whose canopy could reach this chunk.
        int lowX = chunkX * CHUNK_WIDTH - DECORATION_MARGIN;
        int highX = chunkX * CHUNK_WIDTH + CHUNK_WIDTH - 1 + DECORATION_MARGIN;
        int lowZ = chunkZ * CHUNK_WIDTH - DECORATION_MARGIN;
        int highZ = chunkZ * CHUNK_WIDTH + CHUNK_WIDTH - 1 + DECORATION_MARGIN;

        for (int gridX = floorDiv(lowX, TREE_GRID); gridX <= floorDiv(highX, TREE_GRID); gridX++)
        {
            for (int gridZ = floorDiv(lowZ, TREE_GRID); gridZ <= floorDiv(highZ, TREE_GRID); gridZ++)
            {
                placeTree(map, cache, terrain, seed, chunkX, chunkZ, gridX, gridZ);
            }
        }
    }


    // --- everything that stands on a single block ---------------------------

    // Sugar cane wants a column at the waterline with open water beside it.
    // Both are answered from the column cache, so this needs no access to the
    // neighbouring chunks.
    // Whether any of the four columns around this one stands higher than it.
    // A cactus put down next to rising ground would be buried in the side of
    // the dune, so it wants a flat spot.
    bool crowded(const ColumnCache& cache, int localX, int localZ)
    {
        int here = cache.At(localX, localZ).surfaceY;

        return cache.At(localX - 1, localZ).surfaceY > here ||
               cache.At(localX + 1, localZ).surfaceY > here ||
               cache.At(localX, localZ - 1).surfaceY > here ||
               cache.At(localX, localZ + 1).surfaceY > here;
    }

    bool besideWater(const ColumnCache& cache, int localX, int localZ)
    {
        return cache.At(localX - 1, localZ).underwater() ||
               cache.At(localX + 1, localZ).underwater() ||
               cache.At(localX, localZ - 1).underwater() ||
               cache.At(localX, localZ + 1).underwater();
    }

    void placeGroundCover(BlockMap& map, const ColumnCache& cache, uint64_t seed,
                          int chunkX, int chunkZ)
    {
        for (int localX = 0; localX < CHUNK_WIDTH; localX++)
        {
            for (int localZ = 0; localZ < CHUNK_WIDTH; localZ++)
            {
                const ColumnInfo& column = cache.At(localX, localZ);

                int worldX = chunkX * CHUNK_WIDTH + localX;
                int worldZ = chunkZ * CHUNK_WIDTH + localZ;
                int top = column.surfaceY + 1;

                if (top >= CHUNK_HEIGHT)
                    continue;

                // Anything already here is terrain, water, or a tree that grew
                // in from a neighbouring column.
                if (map.Get(localX, top, localZ) != AIR)
                    continue;

                unsigned short ground = map.Get(localX, column.surfaceY, localZ);
                if (!canSupportPlant(ground))
                    continue;

                // Sugar cane first: it wants the waterline, which is exactly
                // where nothing else wants to grow. It comes in stands rather
                // than lining every bank, so a coarse grid picks which
                // stretches of waterline carry one and the rest stay clear.
                if (column.surfaceY == terrain::SEA_LEVEL && besideWater(cache, localX, localZ))
                {
                    WorldRandom stand(seed,
                                      floorDiv(worldX, SUGAR_CANE_PATCH_GRID),
                                      floorDiv(worldZ, SUGAR_CANE_PATCH_GRID),
                                      salt::CANE_PATCH);

                    if (stand.chance(SUGAR_CANE_PATCH_CHANCE))
                    {
                        WorldRandom random(seed, worldX, worldZ, salt::SUGAR_CANE);
                        if (random.chance(SUGAR_CANE_CHANCE))
                        {
                            int height = random.nextInt(1, 3);
                            for (int i = 0; i < height; i++)
                                placeOnAir(map, localX, top + i, localZ, SUGAR_CANE);
                        }
                    }
                    continue;
                }

                // Dry sand, wherever it is. Keying this off the DESERT biome
                // instead meant shrubs needed aridity past a half, while sand
                // starts appearing at a tenth: the whole sandy fringe of every
                // desert came out bare, and shrubs only began some way in.
                // Beaches are left alone, since a dead bush on a riverbank
                // reads as driftwood rather than as desert.
                if (ground == SAND && !column.shore())
                {
                    // A cactus needs room: nothing standing against its sides,
                    // and no ground higher than it next to it, so it does not
                    // end up half buried in the side of a dune.
                    WorldRandom cactus(seed, worldX, worldZ, salt::CACTUS);
                    if (cactus.chance(CACTUS_CHANCE) && !crowded(cache, localX, localZ))
                    {
                        int height = cactus.nextInt(CACTUS_MIN_HEIGHT, CACTUS_MAX_HEIGHT);
                        for (int i = 0; i < height; i++)
                            placeOnAir(map, localX, top + i, localZ, CACTUS);

                        continue;
                    }

                    WorldRandom random(seed, worldX, worldZ, salt::DEAD_SHRUB);
                    if (random.chance(DEAD_SHRUB_CHANCE))
                        placeOnAir(map, localX, top, localZ, DEAD_SHRUB);

                    continue;
                }

                if (ground != GRASS)
                    continue;

                // Pumpkins, in patches. Checked before the smaller plants so a
                // patch is not thinned out by grass winning the column first.
                {
                    WorldRandom patch(seed,
                                      floorDiv(worldX, PUMPKIN_PATCH_GRID),
                                      floorDiv(worldZ, PUMPKIN_PATCH_GRID),
                                      salt::PUMPKIN);

                    if (patch.chance(PUMPKIN_PATCH_CHANCE))
                    {
                        WorldRandom random(seed, worldX, worldZ, salt::PUMPKIN);
                        if (random.chance(PUMPKIN_IN_PATCH_CHANCE))
                        {
                            placeOnAir(map, localX, top, localZ, PUMPKIN);
                            continue;
                        }
                    }
                }

                // Mushrooms only under woodland, standing in for the shade a
                // canopy would cast if there were a light model to ask.
                if (column.forest > 0.5f)
                {
                    WorldRandom random(seed, worldX, worldZ, salt::MUSHROOM);
                    if (random.chance(MUSHROOM_CHANCE))
                    {
                        placeOnAir(map, localX, top, localZ,
                                   random.chance(0.5f) ? RED_MUSHROOM : BROWN_MUSHROOM);
                        continue;
                    }
                }

                {
                    WorldRandom random(seed, worldX, worldZ, salt::FLOWER);
                    if (random.chance(FLOWER_CHANCE * (1.0f - column.aridity)))
                    {
                        WorldRandom patch(seed,
                                          floorDiv(worldX, FLOWER_PATCH_GRID),
                                          floorDiv(worldZ, FLOWER_PATCH_GRID),
                                          salt::FLOWER);

                        placeOnAir(map, localX, top, localZ,
                                   patch.chance(0.5f) ? ROSE : DANDELION);
                        continue;
                    }
                }

                {
                    WorldRandom random(seed, worldX, worldZ, salt::TALL_GRASS);
                    if (random.chance(TALL_GRASS_CHANCE * (1.0f - column.aridity)))
                        placeOnAir(map, localX, top, localZ, LONG_GRASS);
                }
            }
        }
    }
}


void DecorateChunk(BlockMap& map, const TerrainGen& terrain, uint64_t seed,
                   int chunkX, int chunkZ)
{
    ColumnCache cache;
    for (int x = 0; x < PADDED_WIDTH; x++)
    {
        for (int z = 0; z < PADDED_WIDTH; z++)
        {
            int worldX = chunkX * CHUNK_WIDTH + x - DECORATION_MARGIN;
            int worldZ = chunkZ * CHUNK_WIDTH + z - DECORATION_MARGIN;
            cache.columns[x][z] = terrain.ColumnAt(worldX, worldZ);
        }
    }

    // Trees before ground cover, so a plant is never left standing inside a
    // trunk or under a canopy block that arrived after it.
    placeTrees(map, cache, terrain, seed, chunkX, chunkZ);
    placeGroundCover(map, cache, seed, chunkX, chunkZ);
}
