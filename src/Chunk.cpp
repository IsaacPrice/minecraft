#include "headers/Chunk.h"


Chunk::Chunk(int start_x, int start_y) 
{
    chunkPos = { start_x, start_y };
}


void Chunk::Generate(FastNoise &heightGen, FastNoise &gravel, FastNoise &dirt) 
{
    unsigned short heightMap[16][16] = {0};
    for (int i = 0; i < 16; i++) 
    {
        for (int j = 0; j < 16; j++) 
        {
            double worldX = (chunkPos.x * 16 + i) * 3.125f;
            double worldZ = (chunkPos.y * 16 + j) * 3.125f;

            int height = (int)(heightGen.GetNoise(worldX, worldZ) * 30 + 45);
            heightMap[i][j] = height;
        }
    }
    
    for (int x = 0; x < 16; x++) 
    {
        for (int z = 0; z < 16; z++) 
        {
            for (int y = 0; y < 255; y++) 
            {
                if (y <= heightMap[x][z]) 
                {
                    blockMap[x][y][z] = STONE;
                }
                else 
                {
                    blockMap[x][y][z] = AIR;
                }
            }
        }
    }

    for (int x = 0; x < 16; x++) 
    {
        for (int z = 0; z < 16; z++) 
        {
            for (int y = 0; y < 255; y++) 
            {
                if (blockMap[x][y][z] == AIR) 
                {
                    continue;
                }

                double worldX = (chunkPos.x * 16 + x) * 3;
                double worldZ = (chunkPos.y * 16 + z) * 3;

                if (gravel.GetNoise(worldX, (double)(y * 3), worldZ) < 0.3) 
                { 
                    blockMap[x][y][z] = GRAVEL;
                }

                if (dirt.GetNoise(worldX, (double)(y * 3), worldZ) < 0.3) 
                { 
                    blockMap[x][y][z] = DIRT;
                }
            }
        }
    }

    for (int x = 0; x < 16; x++) 
    {
        for (int z = 0; z < 16; z++) 
        {
            for (int y = 0; y < 255; y++) 
            {
                if (blockMap[x][y][z] == AIR) 
                {
                    continue;
                }

                int dirtLayer = 3 + rand() % 2;

                if (y == 0) 
                {
                    blockMap[x][y][z] = BEDROCK;
                }
                else if (y == heightMap[x][z]) 
                {
                    blockMap[x][y][z] = GRASS;
                }
                else if (y >= heightMap[x][z] - dirtLayer) 
                {
                    blockMap[x][y][z] = DIRT;
                }
            }
        }
    }
}


void Chunk::CreateObject() 
{
    _chunk.Create(_vertices, _uvCoords);
}


void Chunk::CleanupPrimatives() 
{
    _vertices.clear();
    _uvCoords.clear();
}


bool Chunk::isChunkSaved() 
{
    return false;
}


void Chunk::Draw() 
{
    _chunk.Draw();
}


void Chunk::MakeVertexObject(Chunk &negativeX, Chunk &positiveX, Chunk &negativeZ, Chunk &positiveZ) 
{
    const float BLOCK_WIDTH = 1.0f / 16.0f;

    for (unsigned x = 0; x < 16; x++) 
    {
        for (unsigned y = 0; y < 255; y++) 
        {
            for (unsigned z = 0; z < 16; z++) 
            {
                int blockID = blockMap[x][y][z];
                if (blockID == 0)
                {
                    continue;
                }

                vec3 blockPos(x * BLOCK_WIDTH, y * BLOCK_WIDTH, z * BLOCK_WIDTH);
                bool sides[] = {false, false, false, false, false, false};

                // TOP
                if (y == 254) 
                {
                    sides[0] = true;
                }
                else if (blockMap[x][y + 1][z] == 0) 
                {
                    sides[0] = true;
                }

                // BOTTOM
                if (y == 0) 
                {
                    sides[1] = true;
                }
                else if (blockMap[x][y - 1][z] == 0) 
                {
                    sides[1] = true;
                }

                // NORTH
                if (x == 0 && negativeX.blockMap[15][y][z] == 0) 
                {
                    sides[2] = true;
                }
                else if (x != 0) 
                {
                    if (blockMap[x - 1][y][z] == 0) 
                    {
                        sides[2] = true;
                    }
                }

                // EAST
                if (z == 15 && positiveZ.blockMap[x][y][0] == 0) 
                {
                    sides[3] = true;
                }
                else if (z != 15) 
                {
                    if (blockMap[x][y][z + 1] == 0) 
                    {
                        sides[3] = true;
                    }
                }

                // SOUTH
                if (x == 15 && positiveX.blockMap[0][y][z] == 0) 
                {
                    sides[4] = true;
                }
                if (x != 15) 
                {
                    if (blockMap[x + 1][y][z] == 0) 
                    {
                        sides[4] = true;
                    }
                }

                // WEST
                if (z == 0 && negativeZ.blockMap[x][y][15] == 0) 
                {
                    sides[5] = true;
                }
                if (z != 0) 
                {
                    if (blockMap[x][y][z - 1] == 0) 
                    {
                        sides[5] = true;
                    }
                }

                for (unsigned i = 0; i < 6; i++) 
                {
                    if (!sides[i])
                    {
                        continue;
                    }

                    vector<vec3> tempVertices = getSideVertex(blockPos.x + chunkPos.x, blockPos.y, blockPos.z + chunkPos.y, (SIDE)i);
                    vector<vec2> tempUV = getTextureCoords((BLOCK)blockID, (SIDE)i);

                    if (_vertices.size() == 0) 
                    {
                        _vertices = tempVertices;
                    }
                    else 
                    {
                        _vertices.insert(_vertices.end(), tempVertices.begin(), tempVertices.end());
                    }

                    if (_uvCoords.size() == 0) 
                    {
                        _uvCoords = tempUV;
                    }
                    else 
                    {
                        _uvCoords.insert(_uvCoords.end(), tempUV.begin(), tempUV.end());
                    }
                }
            }
        }
    }
}
