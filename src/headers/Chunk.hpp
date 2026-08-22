#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "BlockData.hpp"
#include "BlockMap.hpp"
#include "Object.hpp"
#include "Vertex.hpp"

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

    // Size of the meshes built by MakeVertexObject, counted in quad corners --
    // four to a quad. Only meaningful between that call and Cleanup, which drops
    // the vertex data once it is on the GPU.
    size_t SolidVertexCount() const { return _solidVertices.size(); }
    size_t WaterVertexCount() const { return _waterVertices.size(); }

    // Water is meshed separately so it can be drawn after everything else, with
    // blending on and depth writes off.
    void Draw() const;
    void DrawWater() const;

    bool Empty() const { return _solid.Empty() && _water.Empty(); }

    // The box this chunk's geometry actually occupies, in world units. The
    // vertical extent is measured rather than assumed: a chunk is eight units
    // tall, but its terrain only ever fills two or three of them, and handing
    // the frustum the full column would keep chunks that are nowhere near the
    // view. Survives Cleanup, which drops the vertex data these came from.
    const glm::vec3& BoundsLow() const { return _boundsLow; }
    const glm::vec3& BoundsHigh() const { return _boundsHigh; }

    bool operator==(const Chunk& other);

    glm::vec2 chunkPos;

    // Shared with the generator's neighbour cache, so this is a pointer copy
    // rather than 128 KB every time a Chunk moves between queues.
    BlockMapPtr blocks;

private:
    void SetBounds(int lowestBlockY, int highestBlockY);

    Object _solid;
    Object _water;

    glm::vec3 _boundsLow = glm::vec3(0.0f);
    glm::vec3 _boundsHigh = glm::vec3(0.0f);

    std::vector<Vertex> _solidVertices;
    std::vector<Vertex> _waterVertices;
};
