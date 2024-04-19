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
    // Object triangle1(g_vertex_buffer_data, g_uv_buffer_data, sizeof(g_vertex_buffer_data), sizeof(g_uv_buffer_data), programID, "content/finally.png");
    // Create a new chunk object
    chunk = new Object();
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
                    startX, startY + width
                };

                vector<unsigned int> tempIndices = {
                    // Front face
                    0, 1, 2, 
                    2, 3, 0, 
                    // Right face
                    1, 5, 6,
                    6, 2, 1,
                    // Left face
                    4, 0, 3,
                    3, 7, 4,
                    // Top face
                    3, 2, 6,
                    6, 7, 3,
                    // Bottom face
                    4, 5, 1,
                    1, 0, 4,
                    // Back face
                    5, 4, 7,
                    7, 6, 5
                };

                for (unsigned i = 0; i < 6; i++) {
                    uvData.insert(uvData.end(), temp, temp + 6);
                    // Add the indices
                    for (unsigned j = 0; j < 6; j++) {
                        indices.push_back(tempIndices[j] + i * 6);
                    }
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