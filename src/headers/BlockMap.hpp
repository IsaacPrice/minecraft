#pragma once

#include <memory>

#include "BlockData.hpp"

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 255;

// The block ids for one chunk column of the world.
//
// This is held behind a shared_ptr rather than living inside Chunk. Meshing a
// chunk has to read the four chunks around it, so the generator keeps a cache
// of block maps for chunks that have been generated but not yet meshed, and a
// meshed chunk then shares that same allocation instead of copying it. It also
// takes the 128 KB copy out of moving a Chunk between the worker queue and the
// chunk map, which used to happen twice for every chunk that streamed in.
struct BlockMap
{
    unsigned short blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH] = { AIR };

    unsigned short Get(int x, int y, int z) const
    {
        return blocks[x][y][z];
    }

    void Set(int x, int y, int z, unsigned short block)
    {
        blocks[x][y][z] = block;
    }

    // Bounds-checked write, for decoration that spills out of the chunk it is
    // rooted in. Callers place a whole tree and let the parts that land in the
    // neighbouring chunk fall on the floor here -- that neighbour places the
    // same tree itself and keeps the half this one dropped.
    void SetIfInside(int x, int y, int z, unsigned short block)
    {
        if (x < 0 || x >= CHUNK_WIDTH || z < 0 || z >= CHUNK_WIDTH)
            return;
        if (y < 0 || y >= CHUNK_HEIGHT)
            return;

        blocks[x][y][z] = block;
    }
};

typedef std::shared_ptr<BlockMap> BlockMapPtr;
typedef std::shared_ptr<const BlockMap> ConstBlockMapPtr;
