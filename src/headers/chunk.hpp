#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "headers/object.hpp"

using namespace std;

class Chunk {
public:
    Chunk();
    //~Chunk();

    // create chunk


    // Engine Functions
    void MakeVertexObject();

    // Coding Functions
    void DoubleLoop(void (*function)(int x, int y));

private:
    int blockMap[16][64][16] = {0}; // 16 wide, 64 tall, and 16 long
    GLfloat* vertices;
    Glfloat* uvCoords;
};

Chunk::Chunk() {
    // This is the temperary terrain generation
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            blockMap[i][0][j] = 3;
        }
    }

    MakeVertexObject();
    // Object triangle1(g_vertex_buffer_data, g_uv_buffer_data, sizeof(g_vertex_buffer_data), sizeof(g_uv_buffer_data), programID, "content/finally.png");
    Object chunk(*vertices, *uvCoords, sizeof(*vertices), sizeof(*uvCoords), programID, "content/finally.png");
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
                glm::vec3 blockPos(x, y, z);

                // Add vertices for the block
                for (int dx = -1; dx <= 1; dx += 2) {
                    for (int dy = -1; dy <= 1; dy += 2) {
                        for (int dz = -1; dz <= 1; dz += 2) {
                            // Calculate the position of the vertex
                            glm::vec3 vertexPos = blockPos + glm::vec3(dx, dy, dz) * 0.5f;

                            // Add the vertex to the data
                            vertexData.push_back(vertexPos.x / 16.f);
                            vertexData.push_back(vertexPos.y / 16.f);
                            vertexData.push_back(vertexPos.z / 16.f);
                        }
                    }
                }

                float width = 1.0f / 16.0f;

                // Add the uv Coords
                GLfloat temp[] = {
                    startX, startY + width,
                    startX + width, startY + width,
                    startX + width, startY,
                    startX + width, startY,
                    startX, startY,
                    startX, startY + width;
                }

                for (unsigned i = 0; i < 6; i++) {
                    uvData.insert(uvData.end(), temp, temp + 6);
                }
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