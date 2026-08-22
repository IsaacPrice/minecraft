#pragma once

#include <memory>

#include "BlockData.hpp"

const int CHUNK_WIDTH = 16;

// The height field spans roughly y = 25 to y = 65 and the tallest thing put on
// top of it is a six block tree, so nothing has ever reached much past 75. The
// column used to run to 255 anyway, which cost a byte of memory, a byte of
// clearing and a step of the meshing loop for every one of those empty blocks.
const int CHUNK_HEIGHT = 128;

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
    // One byte a block, not two. The highest id in BlockData.hpp is 209, so the
    // second byte was always zero; dropping it halves the memory, the clearing
    // cost, and the amount of cache the mesher has to pull through to walk a
    // chunk. Together with the shorter column this takes a block map from
    // 130 KB to 32 KB.
    unsigned char blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_WIDTH] = { AIR };

    // The highest block in the chunk that is not air. Terrain tops out around
    // y = 65 and the tallest tree reaches into the seventies, so the mesher was
    // walking fifty-odd empty layers of every column to find nothing in them.
    int TopY() const
    {
        return _topY;
    }

    unsigned short Get(int x, int y, int z) const
    {
        return blocks[x][y][z];
    }

    void Set(int x, int y, int z, unsigned short block)
    {
        blocks[x][y][z] = static_cast<unsigned char>(block);

        if (block != AIR && y > _topY)
            _topY = y;
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

        Set(x, y, z, block);
    }

private:
    int _topY = 0;
};

typedef std::shared_ptr<BlockMap> BlockMapPtr;
typedef std::shared_ptr<const BlockMap> ConstBlockMapPtr;
