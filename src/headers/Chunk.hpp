#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "BlockData.hpp"
#include "BlockMap.hpp"
#include "Object.hpp"

class Chunk {
public:
    Chunk();
    Chunk(int start_x, int start_y);

    // Meshes this chunk against the four around it. The neighbours are real
    // block maps now rather than a shared blank stand-in: meshing against blank
    // neighbours drew every face along a chunk border, which was invisible while
    // the world was solid stone but would have put a wall of water surfaces down
    // every chunk seam once rivers existed.
    void MakeVertexObject(const BlockMap& negativeX, const BlockMap& positiveX,
                          const BlockMap& negativeZ, const BlockMap& positiveZ);

    void CreateObject();
    void Cleanup();
    bool isChunkSaved();

    // Size of the meshes built by MakeVertexObject. Only meaningful between
    // that call and Cleanup, which drops the vertex data once it is on the GPU.
    size_t SolidVertexCount() const { return _solidVertices.size(); }
    size_t WaterVertexCount() const { return _waterVertices.size(); }

    // Water is meshed separately so it can be drawn after everything else, with
    // blending on and depth writes off.
    void Draw();
    void DrawWater();

    bool operator==(const Chunk& other);

    glm::vec2 chunkPos;

    // Shared with the generator's neighbour cache, so this is a pointer copy
    // rather than 128 KB every time a Chunk moves between queues.
    BlockMapPtr blocks;

private:
    void AppendFace(bool water, const std::vector<glm::vec3>& faceVertices,
                    const std::vector<glm::vec2>& faceUvs);

    Object _solid;
    Object _water;

    std::vector<glm::vec3> _solidVertices;
    std::vector<glm::vec2> _solidUvs;
    std::vector<glm::vec3> _waterVertices;
    std::vector<glm::vec2> _waterUvs;
};

// Declared here, implemented in Chunk.cpp
std::vector<glm::vec3> getSideVertex(float x, float y, float z, SIDE side);
std::vector<glm::vec3> getCrossVertex(float x, float y, float z);
std::vector<glm::vec3> getCactusVertex(float x, float y, float z, SIDE side);
std::vector<glm::vec2> getTextureCoords(BLOCK block, SIDE side);
std::vector<glm::vec2> insetTile(const std::vector<glm::vec2>& uvs);

extern float blockWidth;
