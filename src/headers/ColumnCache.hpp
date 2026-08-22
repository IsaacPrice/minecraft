#pragma once

#include <algorithm>

#include "BlockMap.hpp"
#include "TerrainGen.hpp"

// How far outside its own chunk a feature rooted elsewhere can reach. Set by
// the widest thing that gets placed, which is a tree canopy at two blocks out
// from the trunk.
const int DECORATION_MARGIN = 2;

const int PADDED_WIDTH = CHUNK_WIDTH + 2 * DECORATION_MARGIN;

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
