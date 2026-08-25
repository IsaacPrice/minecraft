#pragma once

#include <algorithm>

#include "BlockMap.hpp"
#include "TerrainGen.hpp"

// How far outside its own chunk a feature rooted elsewhere can reach. Set by
// the widest thing that gets placed, which is a tree canopy at two blocks out
// from the trunk.
const int DECORATION_MARGIN = 2;

const int PADDED_WIDTH = CHUNK_WIDTH + 2 * DECORATION_MARGIN;

// The beach test asks whether any column within BEACH_RADIUS holds water, and it
// is asked for every column in the padded region -- so the heights it reads run
// that much wider again.
const int HEIGHT_WIDTH = PADDED_WIDTH + 2 * terrain::BEACH_RADIUS;

// The terrain of every column a chunk might need, including the margin outside
// it. Worked out once per chunk and shared by everything that reads it.
//
// This used to be built inside the decorator, which meant every column was run
// through the noise twice: once by the fill, which needs its own 16 x 16, and
// again by the decorator, which needs those plus a two column margin. Filling
// also needs to know how tall the columns around it are, to work out how deep
// its own can be seen, so there are now three callers for one answer.
struct ColumnCache
{
    ColumnInfo columns[PADDED_WIDTH][PADDED_WIDTH];

    // Surface height alone, over a wider area than the columns above, so the
    // beach test is a handful of array reads rather than a fresh trip through
    // the noise for every one of the fifty columns around each candidate.
    // Sampling it per column instead cost two and a half times the whole of
    // generation, because the discs of neighbouring columns overlap almost
    // entirely and every one of those samples was being recomputed.
    int heights[HEIGHT_WIDTH][HEIGHT_WIDTH];

    // Indexed in the same chunk-local coordinates as At.
    int HeightAt(int localX, int localZ) const
    {
        int x = std::min(std::max(localX + DECORATION_MARGIN + terrain::BEACH_RADIUS, 0), HEIGHT_WIDTH - 1);
        int z = std::min(std::max(localZ + DECORATION_MARGIN + terrain::BEACH_RADIUS, 0), HEIGHT_WIDTH - 1);
        return heights[x][z];
    }

    // Dry ground low enough to be beach, with water actually within reach of it.
    // Height alone used to be the whole test, which is why flat plains that had
    // never seen water came out as sand.
    bool NearWater(int localX, int localZ) const
    {
        // Walked outwards a ring at a time, so water right there is found in a
        // few reads and only a column with none near it walks the whole disc.
        for (int radius = 1; radius <= terrain::BEACH_RADIUS; radius++)
        {
            for (int dz = -radius; dz <= radius; dz++)
            {
                for (int dx = -radius; dx <= radius; dx++)
                {
                    // Only the ring just reached; the inside was covered already.
                    if ((dx > -radius && dx < radius) && (dz > -radius && dz < radius))
                        continue;

                    if (HeightAt(localX + dx, localZ + dz) < terrain::SEA_LEVEL)
                        return true;
                }
            }
        }

        return false;
    }

    // Indexed in chunk-local coordinates, which run from -DECORATION_MARGIN
    // to CHUNK_WIDTH + DECORATION_MARGIN - 1.
    const ColumnInfo& At(int localX, int localZ) const
    {
        int x = std::min(std::max(localX + DECORATION_MARGIN, 0), PADDED_WIDTH - 1);
        int z = std::min(std::max(localZ + DECORATION_MARGIN, 0), PADDED_WIDTH - 1);
        return columns[x][z];
    }

    // The lowest of the four columns around this one. A block in this column is
    // only ever visible from the side if it sits above the ground next to it,
    // so this is what bounds how much of the column has to be worked out
    // properly and how much can be left as plain stone.
    int LowestNeighbour(int localX, int localZ) const
    {
        int lowest = At(localX - 1, localZ).surfaceY;
        lowest = std::min(lowest, At(localX + 1, localZ).surfaceY);
        lowest = std::min(lowest, At(localX, localZ - 1).surfaceY);
        lowest = std::min(lowest, At(localX, localZ + 1).surfaceY);
        return lowest;
    }
};
