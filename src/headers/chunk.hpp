#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <cstdlib>

#include "object.hpp"
#include "block_data.hpp"
#include "fast_noise.hpp"
#include "chunk_helper.hpp"

extern GLuint programID;

using namespace std;
using namespace glm;

class Chunk {
public:
    Chunk() {};
    Chunk(int start_x, int start_y);

    // create chunk
    void Generate(FastNoise &heightGen, FastNoise &gravel, FastNoise &dirt);
    void CreateObject();
    void Cleanup();

    // Getters
    bool isChunkSaved();

    // Create the vertex object
    void MakeVertexObject(Chunk &negativeX, Chunk &positiveX, Chunk &negativeZ, Chunk &positiveZ);

    // Draw the chunk
    void Draw();

    // The world position of the chunk
    vec2 chunkPos;

    // The block data of the chunk
    unsigned short blockMap[16][255][16] = { AIR };

private:

    // The object that gets rendered
    Object chunk;

    // The vertices and uvCoords, only for the VBO generation
    vector<vec3> vertices;
    vector<vec2> uvCoords;
};

// Constructor for the chunk
Chunk::Chunk(int start_x, int start_y) {
    chunkPos = { start_x, start_y };
}

// Generate the chunk
void Chunk::Generate(FastNoise &heightGen, FastNoise &gravel, FastNoise &dirt) {
    
    // Generate the height of each x,z
    unsigned short heightMap[16][16] = {0};
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            double worldX = (chunkPos.x * 16 + i) * 3.125f;
            double worldZ = (chunkPos.y * 16 + j) * 3.125f;
            int height = (int)(heightGen.GetNoise(worldX, worldZ) * 30 + 45);
            heightMap[i][j] = height;
        }
    }

    // Generate the Stone Layer
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            for (int y = 0; y < 255; y++) {
                if (y <= heightMap[x][z]) {
                    blockMap[x][y][z] = STONE;
                }
                else {
                    blockMap[x][y][z] = AIR;
                }
            }
        }
    }

    // Generate Patches of gravel
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            for (int y = 0; y < 255; y++) {
                if (blockMap[x][y][z] == AIR)
                    continue;

                double worldX = (chunkPos.x * 16 + x) * 3;
                double worldZ = (chunkPos.y * 16 + z) * 3;

                if (gravel.GetNoise(worldX, (double)(y * 3), worldZ) < 0.3) { 
                    blockMap[x][y][z] = GRAVEL;
                }
            }
        }
    }

    // Generate Patches of Dirt
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            for (int y = 0; y < 255; y++) {
                if (blockMap[x][y][z] == AIR)
                    continue;

                double worldX = (chunkPos.x * 16 + x) * 3;
                double worldZ = (chunkPos.y * 16 + z) * 3;

                if (dirt.GetNoise(worldX, (double)(y * 3), worldZ) < 0.3) { 
                    blockMap[x][y][z] = DIRT;
                }
            }
        }
    }

    // Add in the dirt and grass
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            for (int y = 0; y < 255; y++) {
                if (blockMap[x][y][z] == AIR)
                    continue;

                // generate a random number from 3 to 4
                int dirtLayer = 3 + rand() % 2;
                if (y == 0) {
                    blockMap[x][y][z] = BEDROCK;
                }
                else if (y == heightMap[x][z]) {
                    blockMap[x][y][z] = GRASS;
                }
                else if (y >= heightMap[x][z] - dirtLayer) {
                    blockMap[x][y][z] = DIRT;
                }
            }
        }
    }
}

// Creates the object for the chunk
void Chunk::CreateObject() {
    chunk.Create(vertices, uvCoords);
}

// Deletes the vertices and uvCoords
void Chunk::Cleanup() {
    vertices.clear();
    uvCoords.clear();
}

// Returns if the chunk has been saved
bool Chunk::isChunkSaved() {
    return false;
}

// Draw the chunk
void Chunk::Draw() {
    chunk.Draw();
}

// Go through the block map and create the vertices and uvCoords for the VBO. Currently only creates the faces that face air, but later will cull faces from the surrounding chunks
void Chunk::MakeVertexObject(Chunk &negativeX, Chunk &positiveX, Chunk &negativeZ, Chunk &positiveZ) {

    // Go down up, adding all corners to the array
    for (unsigned x = 0; x < 16; x++) {
        for (unsigned y = 0; y < 255; y++) {
            for (unsigned z = 0; z < 16; z++) {
                // Ignore the block if its air
                int blockID = blockMap[x][y][z];
                if (blockID == 0)
                    continue;

                // Calculate the world position of the block
                vec3 blockPos(x / 16.0f, y / 16.0f, z / 16.0f);

                // Check if the block intersects with air
                bool sides[] = {false, false, false, false, false, false};

                // TOP
                if (y == 254) {
                    sides[0] = true;
                }
                else if (blockMap[x][y + 1][z] == 0) {
                    sides[0] = true;
                }

                // BOTTOM
                if (y == 0) {
                    sides[1] = true;
                }
                else if (blockMap[x][y - 1][z] == 0) {
                    sides[1] = true;
                }

                // NORTH
                if (x == 0 && negativeX.blockMap[15][y][z] == 0) {
                    sides[2] = true;
                }
                else if (x!=0) {
                    if (blockMap[x - 1][y][z] == 0) {
                        sides[2] = true;
                    }
                }

                // EAST
                if (z == 15 && positiveZ.blockMap[x][y][0] == 0) {
                    sides[3] = true;
                }
                else if (z != 15) {
                    if (blockMap[x][y][z + 1] == 0) {
                        sides[3] = true;
                    }
                }

                // SOUTH
                if (x == 15 && positiveX.blockMap[0][y][z] == 0) {
                    sides[4] = true;
                }
                if (x != 15) {
                    if (blockMap[x + 1][y][z] == 0) {
                        sides[4] = true;
                    }
                }

                // WEST
                if (z == 0 && negativeZ.blockMap[x][y][15] == 0) {
                    sides[5] = true;
                }
                if (z != 0) {
                    if (blockMap[x][y][z - 1] == 0) {
                        sides[5] = true;
                    }
                }

                // Add the vertices to the array
                for (unsigned i = 0; i < 6; i++) {
                    if (!sides[i])
                        continue;

                    // Get the vertices and UV coords of the block
                    vector<vec3> tempVertices = getSideVertex(blockPos.x + chunkPos.x, blockPos.y, blockPos.z + chunkPos.y, (SIDE)i);
                    vector<vec2> tempUV = getTextureCoords((BLOCK)blockID, (SIDE)i);

                    if (vertices.size() == 0) {
                        vertices = tempVertices;
                    }
                    else {
                        vertices.insert(vertices.end(), tempVertices.begin(), tempVertices.end());
                    }

                    if (uvCoords.size() == 0) {
                        uvCoords = tempUV;
                    }
                    else {
                        uvCoords.insert(uvCoords.end(), tempUV.begin(), tempUV.end());
                    }
                }
            }
        }
    }
}