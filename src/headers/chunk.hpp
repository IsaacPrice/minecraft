#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "object.hpp"

extern GLuint programID;

using namespace std;
using namespace glm;

// This will be for appending vertices
enum SIDE {
    TOP,
    BOTTOM, 
    NORTH,
    EAST,
    SOUTH,
    WEST,
    NO_SIDE
};

enum BLOCK {
    AIR,
    GRASS,
    STONE,
    DIRT,
    GRASS_SIDE,
    OAK_PLANKS,
    SMOOTH_STONE_SLAB,
    SMOOTH_STONE,
    BRICKS,
    TNT_SIDE,
    TNT_TOP,
    TNT_BOTTOM,
    COBWEB,
    ROSE,
    DANDELION,
    WATER,
    OAK_SAPLING,
    COBBLESTONE,
    BEDROCK,
    SAND,
    GRAVEL,
    OAK_LOG_SIDE,
    OAK_LOG_TOP,
    IRON_BLOCK,
    GOLD_BLOCK,
    DIAMOND_BLOCK,
    CHEST_TOP,
    CHEST_SIDE,
    CHEST_FRONT,
    RED_MUSHROOM,
    BROWN_MUSHROOM,
    SPRUCE_SAPLING,
    NONE1,
    GOLD_ORE,
    IRON_ORE,
    COAL_ORE,
    BOOKSHELF,
    MOSSY_COBBLESTONE,
    OBSIDIAN,
    GRASS_TOP_THING,
    LONG_GRASS,
    TOP_GRASS_AGAIN,
    DOUBLE_CHEST_FRONT,
    CRAFTING_TABLE_TOP,
    FURNACE_FRONT,
    FURNACE_SIDE,
    DISPENSER_FRONT,
    NONE2,
    SPONGE,
    GLASS,
    DIAMOND_ORE,
    REDSTONE_ORE,
    TRANSPARENT_LEAVES,
    LEAVES,
    STONE_BRICK,
    DEAD_SHRUB,
    FERN,
    DOUBLE_CHEST_BACK,
    CRAFTING_TABLE_SIDE,
    CRAFTING_TABLE_FRONT,
    FURNACE_LIT_FRONT,
    STONE_AGAIN,
};

float blockWidth = 0.0625f;

// Function to return the vertex of a side part
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
    //~Chunk();

    // create chunk
    void Generate(int start_x, int start_y);

    void Draw();

    // Engine Functions
    void MakeVertexObject();

    // Coding Functions
    void DoubleLoop(void (*function)(int x, int y));

private:
    unsigned short blockMap[16][64][16] = {0}; // 16 wide, 64 tall, and 16 long
    Object chunk;
    vector<vec3> vertices;
    vector<vec2> uvCoords;
    vec2 chunkPos;
};

Chunk::Chunk(int start_x, int start_y) {
    Generate(start_x, start_y);
}

void Chunk::Generate(int start_x, int start_y) {
    // Generate the height of each x,z
    unsigned short heightMap[16][16] = {0};
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            int height = rand() % 8 + 16;
            heightMap[i][j] = height;
        }
    }

    // Smooth the heightmap n_times
    int n_times = 3;
    for (int i = 0; i < n_times; i++) {
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                int total = 0;
                int count = 0;
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        if (x + dx < 0 || x + dx >= 16 || z + dz < 0 || z + dz >= 16)
                            continue;
                        total += heightMap[x + dx][z + dz];
                        count++;
                    }
                }
                heightMap[x][z] = total / count;
            }
        }
    }

    // Generate the block map
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            int dirt_height = heightMap[x][z] - (3 + (rand() % 2));
            for (int y = 0; y < 64; y++) {
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

    chunkPos = {(float)start_x, (float)start_y};

    MakeVertexObject();

    // Create a new chunk object
    chunk.Create(vertices, uvCoords);

    // Delete the vertices and uvCoords
    vertices.clear();
    uvCoords.clear();
}

void Chunk::Draw() {
    // Draw the chunk
    chunk.Draw();
}

void Chunk::MakeVertexObject() {

    // Go down up, adding all corners to the array
    for (unsigned x = 0; x < 16; x++) {
        for (unsigned y = 0; y < 64; y++) {
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
                if (y == 63) {
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
                if (x == 0) {
                    sides[2] = true;
                }
                else if (blockMap[x - 1][y][z] == 0) {
                    sides[2] = true;
                }
                // EAST
                if (z == 15) {
                    sides[3] = true;
                }
                else if (blockMap[x][y][z + 1] == 0) {
                    sides[3] = true;
                }
                // SOUTH
                if (x == 15) {
                    sides[4] = true;
                }
                else if (blockMap[x + 1][y][z] == 0) {
                    sides[4] = true;
                }
                // WEST
                if (z == 0) {
                    sides[5] = true;
                }
                else if (blockMap[x][y][z - 1] == 0) {
                    sides[5] = true;
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
    printf("VBO generated\n");
}