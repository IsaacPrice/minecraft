#include "headers/Chunk.hpp"

using namespace std;
using namespace glm;

namespace
{
    // Positions are integers in sixteenths of a block, which is the finest
    // thing anything lands on: the inset a cactus is drawn with. A block is
    // sixteen of them, and a chunk is sixteen blocks.
    const int UNITS = VERTEX_UNITS_PER_BLOCK;

    // Which corner of the box each of a face's four vertices takes: 0 for the
    // low corner on that axis, 1 for the high one, indexed [side][vertex][axis].
    //
    // This was a chain of ifs, each returning a freshly built vector of six
    // vec3. Meshing a chunk emits something like eight hundred faces and each
    // one allocated twice -- once for the corners and once for the texture
    // coordinates -- only to copy the result into the mesh and free it again.
    // There are four corners rather than six now because the quads are indexed:
    // the two corners the two triangles share are stored once.
    //
    // The winding is unchanged, and deliberately so: the four side faces are not
    // wound alike, and appendTileUvs below still corrects for it.
    const unsigned char FACE_CORNERS[6][4][3] =
    {
        { {0,1,0}, {0,1,1}, {1,1,1}, {1,1,0} },  // TOP
        { {0,0,0}, {1,0,0}, {1,0,1}, {0,0,1} },  // BOTTOM
        { {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1} },  // NORTH
        { {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} },  // EAST
        { {1,0,0}, {1,1,0}, {1,1,1}, {1,0,1} },  // SOUTH
        { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0} },  // WEST
    };

    // Plants mesh as two quads standing on the block's diagonals rather than as
    // a cube. Both sides of each quad have to be visible.
    //
    // Culling is on now, for the sake of fancy leaves: meshing every face of
    // every leaf block at every depth is a great deal more geometry than a
    // hollow shell, and culling the half of it that faces away pays for it. That
    // needed the box faces wound consistently, which appendQuad now does. The
    // plants keep both sides by being emitted twice, once each way round -- so
    // culling always throws exactly one of the pair away and the pixels drawn
    // come out the same as before. That is preferred to giving the plants a mesh
    // of their own, which would be a third draw call per chunk, and this
    // renderer is bound by draw calls rather than by the triangles in them.
    const unsigned char CROSS_CORNERS[2][4][3] =
    {
        { {0,0,0}, {1,0,1}, {1,1,1}, {0,1,0} },  // corner (x,z) across to (x+1,z+1)
        { {1,0,0}, {0,0,1}, {0,1,1}, {1,1,0} },  // the other diagonal
    };

    // The corner of the tile each vertex samples, in the same two windings the
    // faces come in. NORTH and SOUTH are emitted bottom, top, top, bottom, and
    // the rest bottom, bottom, top, top, so a single list cannot serve both:
    // whichever pair it does not match comes out rotated a quarter turn, which
    // shows on a log, a pumpkin and sandstone.
    const unsigned char TILE_CORNERS[2][4][2] =
    {
        { {1,1}, {0,1}, {0,0}, {1,0} },  // TOP, BOTTOM, EAST, WEST
        { {0,1}, {0,0}, {1,0}, {1,1} },  // NORTH, SOUTH
    };

    bool fancyLeaves = true;

    // Whether a block hides the face of whatever is next to it. Leaves are the
    // one block whose answer depends on the graphics setting: on fast they are
    // an ordinary opaque block and hide their neighbours, on fancy they are
    // see-through and hide nothing.
    bool hidesNeighbourFaces(unsigned short block)
    {
        if (block == LEAVES)
            return !fancyLeaves;

        return isOpaque(block);
    }

    // A face is drawn unless the block beside it hides it. Two blocks of the
    // same see-through kind hide each other, so a body of water does not get a
    // surface meshed between every pair of blocks inside it.
    //
    // Leaves on fancy are the exception, and this is the whole of what makes a
    // canopy look full. A leaf tile is a scatter of leaves with gaps between
    // them, so meshing only the outside of a canopy means looking through those
    // gaps and finding nothing behind -- a hollow shell of leaf-patterned
    // wrapping. Every leaf block drawing all six of its faces puts leaves behind
    // those gaps, at every depth through the tree, and the layers reading
    // against each other is what gives the canopy its density.
    bool showFace(unsigned short block, unsigned short neighbour)
    {
        if (hidesNeighbourFaces(neighbour))
            return false;

        if (block == LEAVES && fancyLeaves)
            return true;

        return block != neighbour;
    }

    // Blocks whose faces do not all share one tile are stored under the id of
    // their side tile, so the differing faces are swapped in here. The aliases
    // at the bottom of BlockData.hpp name the storage ids.
    BLOCK tileFor(BLOCK blockID, SIDE side)
    {
        // The atlas carries a solid leaf tile beside the see-through one. On
        // fast nothing shows through a canopy anyway, and the solid tile costs
        // no cut-out test in the fragment shader.
        if (blockID == LEAVES && !fancyLeaves)
            return LEAVES_OPAQUE;

        if (blockID == GRASS && side == BOTTOM)
            return DIRT;
        if (blockID == GRASS && side != TOP)
            return GRASS_SIDE;
        if (blockID == OAK_LOG_SIDE && (side == TOP || side == BOTTOM))
            return OAK_LOG_TOP;
        if (blockID == PUMPKIN_SIDE && (side == TOP || side == BOTTOM))
            return PUMPKIN_TOP;
        if (blockID == SANDSTONE_SIDE && side == TOP)
            return SANDSTONE_TOP;
        if (blockID == SANDSTONE_SIDE && side == BOTTOM)
            return SANDSTONE_BOTTOM;
        if (blockID == CACTUS_SIDE && side == TOP)
            return CACTUS_TOP;
        if (blockID == CACTUS_SIDE && side == BOTTOM)
            return CACTUS_BOTTOM;

        return blockID;
    }

    // Appends one quad: four corners of a box face, tagged with the atlas tile
    // that face samples and which corner of it each vertex takes.
    //
    // `inset` pulls the sampled area one texel in on every side, which is how
    // the cactus drops the transparent border of its tiles now that its
    // geometry carries the same inset.
    void appendQuad(vector<Vertex>& out,
                    const unsigned char corners[4][3],
                    const int low[3], const int high[3],
                    BLOCK blockID, SIDE side, bool inset,
                    bool reverseWinding, int chunkX, int chunkZ)
    {
        int tile = tileFor(blockID, side) - 1;

        // Read by the vertex shader once it knows how far away the chunk is, and
        // the only thing left that the shader decides for itself.
        bool smallFoliage = isCross(blockID);

        const unsigned char (*tileCorners)[2] = TILE_CORNERS[(side == NORTH || side == SOUTH) ? 1 : 0];

        // NORTH and WEST are listed with their corners going round the other
        // way, so their normals point into the block rather than out of it --
        // which did not matter while face culling was off and is exactly what
        // stopped it being turned on. Walking those four corners backwards fixes
        // the winding without disturbing which tile corner belongs to which
        // position, because both are read at the same index.
        const bool backwards = reverseWinding != (side == NORTH || side == WEST);

        for (int i = 0; i < 4; i++)
        {
            const int v = backwards ? (3 - i) : i;

            Vertex vertex;
            vertex.position = Vertex::PackPosition(corners[v][0] ? high[0] : low[0],
                                                   corners[v][1] ? high[1] : low[1],
                                                   corners[v][2] ? high[2] : low[2]);
            vertex.texture = Vertex::PackTexture(tile, tileCorners[v][0], tileCorners[v][1],
                                                 inset, smallFoliage);
            vertex.chunkX = static_cast<int16_t>(chunkX);
            vertex.chunkZ = static_cast<int16_t>(chunkZ);
            out.push_back(vertex);
        }
    }
}


void SetFancyLeaves(bool fancy)
{
    fancyLeaves = fancy;
}

bool FancyLeaves()
{
    return fancyLeaves;
}


Chunk::Chunk() {}


Chunk::Chunk(int start_x, int start_y) {
    chunkPos = { start_x, start_y };
}


void Chunk::CreateObject()
{
    _solid.Create(_solidVertices);
    _water.Create(_waterVertices);
}


void Chunk::Cleanup()
{
    // Swapping against an empty vector rather than clearing: clear leaves the
    // capacity behind, and a meshed chunk holds tens of kilobytes of it that
    // nothing reads again once the mesh is on the GPU.
    vector<Vertex>().swap(_solidVertices);
    vector<Vertex>().swap(_waterVertices);
}


bool Chunk::isChunkSaved()
{
    return false;
}


void Chunk::Draw() const
{
    _solid.Draw();
}


void Chunk::DrawWater() const
{
    _water.Draw();
}


void Chunk::SetBounds(int lowestBlockY, int highestBlockY)
{
    const float blockWidth = 1.0f / 16.0f;

    // An empty chunk gets a degenerate box at its own corner, which the frustum
    // will happily reject.
    if (lowestBlockY > highestBlockY)
    {
        _boundsLow = glm::vec3(chunkPos.x, 0.0f, chunkPos.y);
        _boundsHigh = _boundsLow;
        return;
    }

    _boundsLow = glm::vec3(chunkPos.x, lowestBlockY * blockWidth, chunkPos.y);
    _boundsHigh = glm::vec3(chunkPos.x + 1.0f, (highestBlockY + 1) * blockWidth, chunkPos.y + 1.0f);
}


void Chunk::MakeVertexObject(const BlockMap& negativeX, const BlockMap& positiveX,
                             const BlockMap& negativeZ, const BlockMap& positiveZ)
{
    const BlockMap& self = *blocks;

    const int chunkX = static_cast<int>(chunkPos.x);
    const int chunkZ = static_cast<int>(chunkPos.y);

    // A typical chunk meshes to a little over three thousand vertices. Reserving
    // for that up front takes a dozen reallocations and copies out of building
    // every single chunk.
    _solidVertices.reserve(4096);

    // Nothing above the tallest block in the chunk can produce a face, and the
    // column runs well past where terrain ever reaches.
    const int topY = self.TopY();

    // Tracked while meshing rather than derived from the vertices afterwards.
    int lowestBlockY = CHUNK_HEIGHT;
    int highestBlockY = 0;

    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        for (int y = 0; y <= topY; y++)
        {
            for (int z = 0; z < CHUNK_WIDTH; z++)
            {
                unsigned short block = self.Get(x, y, z);
                if (block == AIR)
                    continue;

                if (y < lowestBlockY) lowestBlockY = y;
                if (y > highestBlockY) highestBlockY = y;

                // Chunk-local, in sixteenths of a block. The shader adds the
                // chunk's own origin back on.
                const int low[3] = { x * UNITS, y * UNITS, z * UNITS };
                const int high[3] = { low[0] + UNITS, low[1] + UNITS, low[2] + UNITS };

                if (isCross(block))
                {
                    for (int diagonal = 0; diagonal < 2; diagonal++)
                    {
                        appendQuad(_solidVertices, CROSS_CORNERS[diagonal], low, high,
                                   (BLOCK)block, NO_SIDE, false, false, chunkX, chunkZ);
                        appendQuad(_solidVertices, CROSS_CORNERS[diagonal], low, high,
                                   (BLOCK)block, NO_SIDE, false, true, chunkX, chunkZ);
                    }
                    continue;
                }

                bool water = (block == WATER);

                // Nothing is generated above or below the column, so both ends
                // are treated as open air.
                const unsigned short outside = AIR;

                unsigned short neighbours[6];
                neighbours[TOP]    = (y == CHUNK_HEIGHT - 1) ? outside : self.Get(x, y + 1, z);
                neighbours[BOTTOM] = (y == 0)                ? outside : self.Get(x, y - 1, z);
                neighbours[NORTH]  = (x == 0)               ? negativeX.Get(CHUNK_WIDTH - 1, y, z) : self.Get(x - 1, y, z);
                neighbours[EAST]   = (z == CHUNK_WIDTH - 1) ? positiveZ.Get(x, y, 0)               : self.Get(x, y, z + 1);
                neighbours[SOUTH]  = (x == CHUNK_WIDTH - 1) ? positiveX.Get(0, y, z)               : self.Get(x + 1, y, z);
                neighbours[WEST]   = (z == 0)               ? negativeZ.Get(x, y, CHUNK_WIDTH - 1) : self.Get(x, y, z - 1);

                if (block == CACTUS)
                {
                    // A cactus is a block narrower than the one it stands in.
                    // The atlas draws that by leaving a transparent border
                    // around its tiles, which the cut-out threshold would throw
                    // away, slitting the plant open along every corner. Pulling
                    // the geometry in by that same texel instead, and trimming
                    // the border off the texture, gives the same silhouette with
                    // nothing see-through in it. Full block height, so a stack
                    // of them meets cleanly.
                    const int narrowLow[3] = { low[0] + 1, low[1], low[2] + 1 };
                    const int narrowHigh[3] = { high[0] - 1, high[1], high[2] - 1 };

                    for (int side = 0; side < 6; side++)
                    {
                        // Only the faces between two stacked cactus blocks are
                        // hidden. Being narrower than a block, a cactus never
                        // has anything flush against its sides to hide them.
                        if (neighbours[side] == CACTUS)
                            continue;

                        appendQuad(_solidVertices, FACE_CORNERS[side], narrowLow, narrowHigh,
                                   (BLOCK)block, (SIDE)side, true, false, chunkX, chunkZ);
                    }
                    continue;
                }

                for (int side = 0; side < 6; side++)
                {
                    if (!showFace(block, neighbours[side]))
                        continue;

                    appendQuad(water ? _waterVertices : _solidVertices,
                               FACE_CORNERS[side], low, high, (BLOCK)block, (SIDE)side, false,
                               false, chunkX, chunkZ);
                }
            }
        }
    }

    SetBounds(lowestBlockY, highestBlockY);
}


bool Chunk::operator==(const Chunk &other) {
    return chunkPos == other.chunkPos;
}
