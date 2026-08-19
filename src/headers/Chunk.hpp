#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Object.hpp"
#include "FastNoise.hpp"
#include "BlockData.hpp"

class Chunk {
public:
    Chunk();
    Chunk(int start_x, int start_y);

    void Generate(FastNoise& heightGen, FastNoise& gravel, FastNoise& dirt);
    void CreateObject();
    void Cleanup();
    bool isChunkSaved();
    void MakeVertexObject(Chunk& negativeX, Chunk& positiveX, Chunk& negativeZ, Chunk& positiveZ);
    void Draw();

    bool operator==(const Chunk& other);

    glm::vec2 chunkPos;

    // blankChunk stands in for absent neighbours during meshing, and meshing reads
    // this array, so it has to start as air rather than uninitialised memory.
    unsigned short blockMap[16][255][16] = { AIR };

private:
    Object chunk;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvCoords;
};

// Declared here, implemented in Chunk.cpp
std::vector<glm::vec3> getSideVertex(float x, float y, float z, SIDE side);
std::vector<glm::vec2> getTextureCoords(BLOCK block, SIDE side);

extern float blockWidth;
