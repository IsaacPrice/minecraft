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
    WEST
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

float blockWidth = 1 / 16;

// Function to return the vertex of a side part
vector<vec3> getSideVertex(vec3 startingPos, SIDE part) {
    if (part == TOP) {
        return {
            {startingPos.x + 0, startingPos.y + blockWidth, startingPos.z + 0},
            {startingPos.x + 0, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + 0},
            {startingPos.x + 0, startingPos.y + blockWidth, startingPos.z + 0}
        };
    }
    else if (part == BOTTOM) {
        return {
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + 0},
            {startingPos.x + blockWidth, startingPos.y + 0, startingPos.z + 0},
            {startingPos.x + blockWidth, startingPos.y + 0, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + 0, startingPos.z + blockWidth},
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + blockWidth},
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + 0}
        };
    }
    else if (part == NORTH) {
        return {
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + 0},
            {startingPos.x + 0, startingPos.y + blockWidth, startingPos.z + 0},
            {startingPos.x + 0, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + 0, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + blockWidth},
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + 0}
        };
    }
    else if (part == EAST) {
        return {
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + 0, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + 0, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + blockWidth}
        };
    }
    else if (part == SOUTH) {
        return {
            {startingPos.x + blockWidth, startingPos.y + 0, startingPos.z + 0},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + 0},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + 0, startingPos.z + blockWidth},
            {startingPos.x + blockWidth, startingPos.y + 0, startingPos.z + 0}
        };
    }
    else {
        return {
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + 0},
            {startingPos.x + blockWidth, startingPos.y + 0, startingPos.z + 0},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + 0},
            {startingPos.x + blockWidth, startingPos.y + blockWidth, startingPos.z + 0},
            {startingPos.x + 0, startingPos.y + blockWidth, startingPos.z + 0},
            {startingPos.x + 0, startingPos.y + 0, startingPos.z + 0}
        };
    }
}

vector<vec2> getTextureCoords(BLOCK blockID) {
    // Calculate the starting point of the texture
    float startX = (blockID % 16) * 0.0625;
    float startY = (int(blockID / 16)) * 0.0625;

    return {
        {startX, startY},
        {startX + 0.0625, startY},
        {startX + 0.0625, startY + 0.0625},
        {startX + 0.0625, startY + 0.0625},
        {startX, startY + 0.0625},
        {startX, startY}
    };
}

class Chunk {
public:
    Chunk();
    //~Chunk();

    // create chunk

    void DrawChunk();
    // Engine Functions
    void MakeVertexObject();

    // Coding Functions
    void DoubleLoop(void (*function)(int x, int y));

private:
    unsigned short blockMap[16][64][16] = {0}; // 16 wide, 64 tall, and 16 long
    Object* chunk;
    vector<vec3> vertices;
    vector<vec2> uvCoords;
};

Chunk::Chunk() {
    // This is the temperary terrain generation
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            blockMap[i][0][j] = 2;
        }
    }

    MakeVertexObject();

    // Create a new chunk object
    chunk = new Object(vertices, uvCoords);
}

void Chunk::DrawChunk() {
    // Draw the chunk
    chunk->Draw();
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

                // Get the vertices and UV coords of the block
                for (unsigned i = 0; i < 6; i++) {
                    // add the new vectors to the vertices vector
                    vector<vec3> tempVertices = getSideVertex(blockPos, (SIDE)i);
                    vector<vec2> tempUV = getTextureCoords((BLOCK)blockID);

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