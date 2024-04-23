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

// This will be for appending vertices
enum SIDE {
    TOP,
    BOTTOM, 
    NORTH,
    EAST,
    SOUTH,
    WEST
};

float blockWidth = 1 / 16;

// Function to return the vertex of a side part
vector<vec3> getSideVertex(vec3 startingPos, SIDE part) {
    if (part == TOP) {
        return {
            startingPos + {0, blockWidth, 0},
            startingPos + {}
        }
    }
    if (part == BOTTOM) {

    }
    if (part == NORTH) {

    }
    if (part == EAST) {

    }
    if (part == SOUTH) {

    }
    if (part == WEST) {
        
    }
}

using namespace std;

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
    //GLfloat* vertices;
    //GLfloat* uvCoords;
    vector<GLfloat> vertices;
    vector<GLfloat> uvCoords;
    vector<unsigned> indices;
};

Chunk::Chunk() {
    // This is the temperary terrain generation
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            blockMap[i][0][j] = 3;
        }
    }

    GLuint programID = 3;

    MakeVertexObject();

    // Create a new chunk object
    chunk = new Object(vertices, uvCoords);
}

void Chunk::DrawChunk() {
    // Draw the chunk
    chunk->Draw();
}

void Chunk::MakeVertexObject() {
    GLfloat startX = 0.0625, startY = 0;

    // Create the object as a vector, then when its finalized, make it an array
    vector<GLfloat> vertexData;
    vector<GLfloat> uvData;

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

                
            }
        }
    }

    // Append the new vertices to the array
    vertices = new GLfloat[vertexData.size()];
    std::copy(vertexData.begin(), vertexData.end(), vertices);

    // Append the new UV coords to the array
    uvCoords = new GLfloat[uvData.size()];
    std::copy(uvData.begin(), uvData.end(), uvCoords);

}