#include "headers/Chunk.hpp"

using namespace std;
using namespace glm;

float blockWidth = 0.0625f;


// One face of an arbitrary box. Blocks are cubes, but a cactus is narrower than
// a block, so the corners are passed in rather than derived from blockWidth.
static vector<vec3> getBoxFace(float x0, float y0, float z0,
                               float x1, float y1, float z1, SIDE part)
{
    if (part == TOP)
    {
        return
        {
            {x0, y1, z0},
            {x0, y1, z1},
            {x1, y1, z1},
            {x1, y1, z1},
            {x1, y1, z0},
            {x0, y1, z0}
        };
    }
    else if (part == BOTTOM)
    {
        return
        {
            {x0, y0, z0},
            {x1, y0, z0},
            {x1, y0, z1},
            {x1, y0, z1},
            {x0, y0, z1},
            {x0, y0, z0}
        };
    }
    else if (part == NORTH)
    {
        return
        {
            {x0, y0, z0},
            {x0, y1, z0},
            {x0, y1, z1},
            {x0, y1, z1},
            {x0, y0, z1},
            {x0, y0, z0}
        };
    }
    else if (part == EAST)
    {
        return
        {
            {x0, y0, z1},
            {x1, y0, z1},
            {x1, y1, z1},
            {x1, y1, z1},
            {x0, y1, z1},
            {x0, y0, z1}
        };
    }
    else if (part == SOUTH)
    {
        return
        {
            {x1, y0, z0},
            {x1, y1, z0},
            {x1, y1, z1},
            {x1, y1, z1},
            {x1, y0, z1},
            {x1, y0, z0}
        };
    }
    else
    {
        return
        {
            {x0, y0, z0},
            {x1, y0, z0},
            {x1, y1, z0},
            {x1, y1, z0},
            {x0, y1, z0},
            {x0, y0, z0}
        };
    }
}


vector<vec3> getSideVertex(float x, float y, float z, SIDE part)
{
    return getBoxFace(x, y, z, x + blockWidth, y + blockWidth, z + blockWidth, part);
}


// A cactus is a block narrower than the one it stands in. The atlas draws that
// by leaving a transparent border around its tiles, which the cut-out threshold
// would throw away, slitting the plant open along every corner. Pulling the
// geometry in by the same one texel instead, and trimming that border off the
// texture, gives the same silhouette with nothing see-through in it. Full block
// height, so a stack of them meets cleanly.
vector<vec3> getCactusVertex(float x, float y, float z, SIDE part)
{
    float inset = blockWidth / 16.0f;

    return getBoxFace(x + inset, y, z + inset,
                      x + blockWidth - inset, y + blockWidth, z + blockWidth - inset, part);
}


// Pulls texture coordinates in by one texel on every side, dropping the
// transparent border of the cactus tiles now that the geometry carries it.
vector<vec2> insetTile(const vector<vec2>& uvs)
{
    const float texel = 0.0625f / 16.0f;

    float centreU = 0.0f, centreV = 0.0f;
    for (size_t i = 0; i < uvs.size(); i++)
    {
        centreU += uvs[i].x;
        centreV += uvs[i].y;
    }
    centreU /= uvs.size();
    centreV /= uvs.size();

    vector<vec2> pulled;
    pulled.reserve(uvs.size());
    for (size_t i = 0; i < uvs.size(); i++)
    {
        pulled.push_back({ uvs[i].x + (uvs[i].x < centreU ? texel : -texel),
                           uvs[i].y + (uvs[i].y < centreV ? texel : -texel) });
    }

    return pulled;
}


// Plants mesh as two quads standing on the diagonals of the block instead of as
// a cube. Both sides of each quad have to be visible, which works because face
// culling is off. The vertex order matches getSideVertex, so a cross quad can
// reuse the same texture coordinates a cube face would use.
vector<vec3> getCrossVertex(float x, float y, float z)
{
    float w = blockWidth;

    return
    {
        // Diagonal from the (x, z) corner across to (x + w, z + w).
        {x, y, z},
        {x + w, y, z + w},
        {x + w, y + w, z + w},
        {x + w, y + w, z + w},
        {x, y + w, z},
        {x, y, z},

        // Diagonal running the other way.
        {x + w, y, z},
        {x, y, z + w},
        {x, y + w, z + w},
        {x, y + w, z + w},
        {x + w, y + w, z},
        {x + w, y, z}
    };
}


vector<vec2> getTextureCoords(BLOCK blockID, SIDE side)
{
    // The four side faces are not wound the same way. getSideVertex emits north
    // and south as bottom, top, top, top, bottom, bottom, and east and west as
    // bottom, bottom, top, top, top, bottom, so a single list of texture
    // coordinates cannot serve both: whichever pair it does not match comes out
    // rotated a quarter turn. Only the grass side used to correct for this,
    // which was enough while every other side texture was isotropic noise that
    // looks the same rotated. A log, a pumpkin and sandstone are not, and came
    // out with their grain running across some faces and along others.
    bool altCoords = (side == NORTH || side == SOUTH);

    if (blockID == GRASS && side == BOTTOM)
    {
        blockID = DIRT;
    }
    else if (blockID == GRASS && side != TOP)
    {
        blockID = GRASS_SIDE;
    }
    // Blocks whose faces do not all share one tile are stored under the id of
    // their side tile, so the differing faces are swapped in here. The aliases
    // at the bottom of BlockData.hpp name the storage ids.
    else if (blockID == OAK_LOG_SIDE && (side == TOP || side == BOTTOM))
    {
        blockID = OAK_LOG_TOP;
    }
    else if (blockID == PUMPKIN_SIDE && (side == TOP || side == BOTTOM))
    {
        blockID = PUMPKIN_TOP;
    }
    else if (blockID == SANDSTONE_SIDE && side == TOP)
    {
        blockID = SANDSTONE_TOP;
    }
    else if (blockID == SANDSTONE_SIDE && side == BOTTOM)
    {
        blockID = SANDSTONE_BOTTOM;
    }
    else if (blockID == CACTUS_SIDE && side == TOP)
    {
        blockID = CACTUS_TOP;
    }
    else if (blockID == CACTUS_SIDE && side == BOTTOM)
    {
        blockID = CACTUS_BOTTOM;
    }

    float startX = ((blockID - 1) % 16) * 0.0625;
    float startY = (int((blockID - 1) / 16)) * 0.0625;

    if (altCoords)
    {
        return
        {
            {startX, startY + 0.0625},
            {startX, startY},
            {startX + 0.0625, startY},
            {startX + 0.0625, startY},
            {startX + 0.0625, startY + 0.0625},
            {startX, startY + 0.0625},
        };
    }

    return
    {
        {startX + 0.0625, startY + 0.0625},
        {startX, startY + 0.0625},
        {startX, startY},
        {startX, startY},
        {startX + 0.0625, startY},
        {startX + 0.0625, startY + 0.0625},
    };
}


namespace
{
    // A face is drawn unless the block beside it hides it. Two blocks of the
    // same see-through kind hide each other, so a body of water does not get a
    // surface meshed between every pair of blocks inside it, and a canopy does
    // not mesh the inside of itself.
    bool showFace(unsigned short block, unsigned short neighbour)
    {
        if (isOpaque(neighbour))
            return false;

        return block != neighbour;
    }
}


Chunk::Chunk() {}


Chunk::Chunk(int start_x, int start_y) {
    chunkPos = { start_x, start_y };
}


void Chunk::CreateObject()
{
    _solid.Create(_solidVertices, _solidUvs);
    _water.Create(_waterVertices, _waterUvs);
}


void Chunk::Cleanup()
{
    _solidVertices.clear();
    _solidUvs.clear();
    _waterVertices.clear();
    _waterUvs.clear();
}


bool Chunk::isChunkSaved()
{
    return false;
}


void Chunk::Draw()
{
    _solid.Draw();
}


void Chunk::DrawWater()
{
    _water.Draw();
}


void Chunk::AppendFace(bool water, const vector<vec3>& faceVertices, const vector<vec2>& faceUvs)
{
    vector<vec3>& vertices = water ? _waterVertices : _solidVertices;
    vector<vec2>& uvs = water ? _waterUvs : _solidUvs;

    vertices.insert(vertices.end(), faceVertices.begin(), faceVertices.end());
    uvs.insert(uvs.end(), faceUvs.begin(), faceUvs.end());
}


void Chunk::MakeVertexObject(const BlockMap& negativeX, const BlockMap& positiveX,
                             const BlockMap& negativeZ, const BlockMap& positiveZ)
{
    const BlockMap& self = *blocks;

    for (int x = 0; x < CHUNK_WIDTH; x++)
    {
        for (int y = 0; y < CHUNK_HEIGHT; y++)
        {
            for (int z = 0; z < CHUNK_WIDTH; z++)
            {
                unsigned short block = self.Get(x, y, z);
                if (block == AIR)
                    continue;

                float worldX = x / 16.0f + chunkPos.x;
                float worldY = y / 16.0f;
                float worldZ = z / 16.0f + chunkPos.y;

                if (isCross(block))
                {
                    // Two quads, so the six texture coordinates a single quad
                    // needs are laid down twice.
                    vector<vec2> tileUvs = getTextureCoords((BLOCK)block, NO_SIDE);
                    vector<vec2> crossUvs(tileUvs);
                    crossUvs.insert(crossUvs.end(), tileUvs.begin(), tileUvs.end());

                    AppendFace(false, getCrossVertex(worldX, worldY, worldZ), crossUvs);
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
                    for (int side = 0; side < 6; side++)
                    {
                        // Only the faces between two stacked cactus blocks are
                        // hidden. Being narrower than a block, a cactus never
                        // has anything flush against its sides to hide them.
                        if (neighbours[side] == CACTUS)
                            continue;

                        AppendFace(false,
                                   getCactusVertex(worldX, worldY, worldZ, (SIDE)side),
                                   insetTile(getTextureCoords((BLOCK)block, (SIDE)side)));
                    }
                    continue;
                }

                for (int side = 0; side < 6; side++)
                {
                    if (!showFace(block, neighbours[side]))
                        continue;

                    AppendFace(water,
                               getSideVertex(worldX, worldY, worldZ, (SIDE)side),
                               getTextureCoords((BLOCK)block, (SIDE)side));
                }
            }
        }
    }
}


bool Chunk::operator==(const Chunk &other) {
    return chunkPos == other.chunkPos;
}
