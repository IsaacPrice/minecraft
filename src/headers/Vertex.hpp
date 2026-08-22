#pragma once

#include <cstdint>

// One mesh vertex, in twelve bytes rather than twenty.
//
// It used to be a vec3 of absolute world position in one buffer and a vec2 of
// texture coordinates in another. That is twenty bytes a vertex, and at a full
// render distance the visible geometry is several million vertices a frame, for
// numbers that mostly do not need the range or the precision a float gives them.
struct Vertex
{
    // Chunk-local position: x in bits 0-8, y in bits 9-20, z in bits 21-29,
    // each in sixteenths of a block. A chunk is sixteen blocks across and a
    // hundred and twenty eight tall, and the finest thing anything lands on is
    // the sixteenth-of-a-block inset a cactus is drawn with, so every coordinate
    // is an exact integer and nothing is rounded. The vertex shader scales it
    // back down and adds the chunk's origin.
    uint32_t position;

    // Which atlas tile this corner samples, and which corner of it:
    //
    //   bits 0-7   tile index, which is the block id minus one
    //   bit  8     u: 0 for the left edge of the tile, 1 for the right
    //   bit  9     v: 0 for the top edge, 1 for the bottom
    //   bit  10    inset: pull one texel in on all four sides, for the cactus
    //
    // The texture coordinate itself is worked out in the shader rather than
    // stored. Storing it is what caused plants and water to come out with a
    // fringe of the wrong tile around every face: as two normalised sixteen bit
    // integers, a tile edge lands on 65535/16, which is 4095.9375 and not a
    // whole number. Rounding it left every tile about a tenth of a texel adrift,
    // enough that a fragment on the boundary sampled the neighbouring tile --
    // and the neighbour of water in the atlas is the dandelion, which is why the
    // river had yellow specks along its seams. Sixteenths are exact in floating
    // point, so deriving the coordinate cannot drift.
    uint16_t texture;

    // Which chunk this vertex belongs to. The same pair for every vertex of a
    // chunk, which is four bytes of pure repetition -- and still the cheaper way
    // round. The alternative is a uniform set once per chunk before its draw,
    // and measuring that showed it costing about a quarter of a millisecond a
    // frame: at this render distance the renderer is bound by how many calls
    // reach the driver, not by how many bytes reach the card.
    int16_t chunkX;
    int16_t chunkZ;

    static uint32_t PackPosition(int x, int y, int z)
    {
        return static_cast<uint32_t>(x)
             | (static_cast<uint32_t>(y) << 9)
             | (static_cast<uint32_t>(z) << 21);
    }

    static uint16_t PackTexture(int tile, int cornerU, int cornerV, bool inset,
                                bool smallFoliage, bool leaf, bool interiorLeaf)
    {
        return static_cast<uint16_t>(tile
             | (cornerU << 8)
             | (cornerV << 9)
             | ((inset ? 1 : 0) << 10)
             | ((smallFoliage ? 1 : 0) << 11)
             | ((leaf ? 1 : 0) << 12)
             | ((interiorLeaf ? 1 : 0) << 13));
    }
};

static_assert(sizeof(Vertex) == 12, "the vertex format is meant to be twelve bytes");

// How many sixteenths-of-a-block there are in a block. Mirrored by the scale
// constant in shader.vert.
const int VERTEX_UNITS_PER_BLOCK = 16;
