#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>

#include "object.hpp"
#include "block_data.hpp"

//#include "generation.hpp"
#include "fast_noise.hpp"

extern GLuint programID;

using namespace std;
using namespace glm;

// Width of each block
float blockWidth = 0.0625f;

// Returns the vertices for the side of the block requested
vector<vec3> getSideVertex(float x, float y, float z, SIDE part) {
    if (part == TOP) {
        return {
            {x, y + blockWidth, z},
            {x, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z},
            {x, y + blockWidth, z}
        };;
    }
    else if (part == BOTTOM) {
        return {
            {x, y, z},
            {x + blockWidth, y, z},
            {x + blockWidth, y, z + blockWidth},
            {x + blockWidth, y, z + blockWidth},
            {x, y, z + blockWidth},
            {x, y, z}
        };
    }
    else if (part == NORTH) {
        return {
            {x, y, z},
            {x, y + blockWidth, z},
            {x, y + blockWidth, z + blockWidth},
            {x, y + blockWidth, z + blockWidth},
            {x, y, z + blockWidth},
            {x, y, z}
        };
    }
    else if (part == EAST) {
        return {
            {x, y, z + blockWidth},
            {x + blockWidth, y, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x, y + blockWidth, z + blockWidth},
            {x, y, z + blockWidth}
        };
    }
    else if (part == SOUTH) {
        return {
            {x + blockWidth, y, z},
            {x + blockWidth, y + blockWidth, z},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y + blockWidth, z + blockWidth},
            {x + blockWidth, y, z + blockWidth},
            {x + blockWidth, y, z}
        };
    }
    else {
        return {
            {x, y, z},
            {x + blockWidth, y, z},
            {x + blockWidth, y + blockWidth, z},
            {x + blockWidth, y + blockWidth, z},
            {x, y + blockWidth, z},
            {x, y, z}
        };
    }
}

// Returns the texture coordinates for the block requested
vector<vec2> getTextureCoords(BLOCK blockID, SIDE side) {
    bool altCoords = false;

    if (blockID == GRASS && side == BOTTOM){
        blockID = DIRT;
    }
    else if (blockID == GRASS && side != TOP) {
        blockID = GRASS_SIDE;
        if (side == NORTH || side == SOUTH)
            altCoords = true;
    }

    // Calculate the starting point of the texture
    float startX = ((blockID - 1) % 16) * 0.0625;
    float startY = (int((blockID - 1) / 16)) * 0.0625;

    if (altCoords) {
        return {
            {startX, startY + 0.0625},
            {startX, startY},
            {startX + 0.0625, startY},
            {startX + 0.0625, startY},
            {startX + 0.0625, startY + 0.0625},
            {startX, startY + 0.0625},
        };
    }

    return {
        {startX + 0.0625, startY + 0.0625},
        {startX, startY + 0.0625},
        {startX, startY},
        {startX, startY},
        {startX + 0.0625, startY},
        {startX + 0.0625, startY + 0.0625},
    };
}

class Chunk {
public:
    Chunk() {};
    Chunk(int start_x, int start_y);

    // create chunk
    void Generate(FastNoise &noise);
    void CreateObject();
    void Cleanup();

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

    //Compiling shader : src/shaders/shader.frag
    // The vertices and uvCoords, only for the VBO generation
    vector<vec3> vertices;
    vector<vec2> uvCoords;
};

// Constructor for the chunk
Chunk::Chunk(int start_x, int start_y) {
    chunkPos = { start_x, start_y };
}

// Generate the chunk
void Chunk::Generate(FastNoise &noise) {
    // Generate the height of each x,z
    unsigned short heightMap[16][16] = {0};
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            double worldX = (chunkPos.x * 16 + i) * 1.5f;
            double worldZ = (chunkPos.y * 16 + j) * 1.5f;
            int height = (int)(noise.GetNoise(worldX, worldZ) * 20 + 58);
            heightMap[i][j] = height;
        }
    }

    // Generate the block map
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            int dirt_height = heightMap[x][z] - (3 + (rand() % 2));
            for (int y = 0; y < 255; y++) {
                if (y == 0) {
                    blockMap[x][y][z] = BEDROCK;
                }
                else if (y < dirt_height) {
                    blockMap[x][y][z] = STONE;
                }
                else if (y < heightMap[x][z]) {
                    blockMap[x][y][z] = DIRT;
                }
                else if (y == heightMap[x][z]) {
                    blockMap[x][y][z] = GRASS;
                }
                else {
                    blockMap[x][y][z] = AIR;
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