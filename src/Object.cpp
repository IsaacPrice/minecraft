#include "headers/Object.hpp"

#include <cstddef>
#include <cstdio>
#include <utility>
#include <vector>

#include "headers/TextureAtlas.hpp"

namespace
{
    // The block atlas is shared by every chunk, so it is uploaded once and reused.
    GLuint terrainTexture = 0;
    GLint terrainSamplerUniform = -1;

    // Every quad in every chunk is indexed the same way -- four corners, two
    // triangles, 0 1 2 2 3 0 -- so one element buffer serves the whole world
    // instead of each chunk carrying a copy. Indexing at all is worth four
    // vertices a quad instead of six: the two shared corners used to be written
    // out, uploaded and transformed twice over.
    GLuint sharedIndexBuffer = 0;
    size_t sharedIndexQuads = 0;

    // Grows the shared index buffer to cover at least this many quads. The GL
    // name never changes, only the storage behind it, which matters because
    // every chunk's vertex array records that name and would otherwise be left
    // pointing at a buffer that had been deleted.
    void ensureIndexCapacity(size_t quads)
    {
        if (quads <= sharedIndexQuads)
            return;

        size_t capacity = sharedIndexQuads ? sharedIndexQuads : 2048;
        while (capacity < quads)
            capacity *= 2;

        std::vector<GLuint> indices;
        indices.reserve(capacity * 6);
        for (size_t quad = 0; quad < capacity; quad++)
        {
            GLuint base = static_cast<GLuint>(quad * 4);
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
            indices.push_back(base + 0);
        }

        if (sharedIndexBuffer == 0)
            glGenBuffers(1, &sharedIndexBuffer);

        // Filled through the array target rather than the element target on
        // purpose: the element binding belongs to whichever vertex array is
        // current, and filling the buffer here must not disturb one.
        glBindBuffer(GL_ARRAY_BUFFER, sharedIndexBuffer);
        glBufferData(GL_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        sharedIndexQuads = capacity;
    }
}

void InitRenderResources()
{
    if (terrainTexture != 0)
        return;

    terrainTexture = LoadBlockAtlasArray("content/terrain.png", 16);
    terrainSamplerUniform = glGetUniformLocation(programID, "myTextureSampler");
}

void BindTerrainTexture()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, terrainTexture);
    glUniform1i(terrainSamplerUniform, 0);
}

Object::Object(const std::vector<Vertex>& vertices)
{
    Create(vertices);
}

Object::~Object()
{
    release();
}

Object::Object(Object&& other) noexcept
    : VertexArrayID(other.VertexArrayID),
      vertexBuffer(other.vertexBuffer),
      indexCount(other.indexCount)
{
    other.VertexArrayID = 0;
    other.vertexBuffer = 0;
    other.indexCount = 0;
}

Object& Object::operator=(Object&& other) noexcept
{
    if (this == &other)
        return *this;

    release();

    VertexArrayID = other.VertexArrayID;
    vertexBuffer = other.vertexBuffer;
    indexCount = other.indexCount;

    other.VertexArrayID = 0;
    other.vertexBuffer = 0;
    other.indexCount = 0;

    return *this;
}

void Object::release()
{
    // Chunks are built on worker threads, which have no GL context, and are
    // destroyed there if the world shuts down mid-build. Those Objects never
    // had buffers generated, so returning early keeps GL calls on the render
    // thread where they belong.
    if (VertexArrayID == 0 && vertexBuffer == 0)
    {
        indexCount = 0;
        return;
    }

    glDeleteBuffers(1, &vertexBuffer);
    glDeleteVertexArrays(1, &VertexArrayID);

    vertexBuffer = 0;
    VertexArrayID = 0;
    indexCount = 0;
}

void Object::Create(const std::vector<Vertex>& vertices)
{
    // Create() is called again when the render distance changes, so drop any
    // buffers this Object already owns rather than leaking them.
    release();

    if (vertices.empty())
        return;

    size_t quads = vertices.size() / 4;
    indexCount = static_cast<GLsizei>(quads * 6);

    ensureIndexCapacity(quads);

    // The attribute layout is recorded into this chunk's own vertex array
    // object, so drawing is a bind and a draw rather than respecifying every
    // pointer. That is not just faster: the old code never bound a vertex array
    // at all in Draw, and leaned on whichever one Create happened to leave bound.
    // Deleting a bound vertex array reverts the binding to zero, and in a core
    // profile drawing with no vertex array bound is an error that draws nothing,
    // so unloading the chunk that owned the bound array turned every remaining
    // draw that frame into a no-op and the screen went black for a frame.
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // Position is an integer the shader unpacks, so it takes the integer
    // attribute path. The ordinary one would convert it to a float on the way
    // in and lose the packing.
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(Vertex),
                           (void*)offsetof(Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_SHORT, sizeof(Vertex),
                           (void*)offsetof(Vertex, texture));

    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 2, GL_SHORT, sizeof(Vertex),
                           (void*)offsetof(Vertex, chunkX));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedIndexBuffer);

    glBindVertexArray(0);
}

void Object::Draw() const
{
    if (indexCount == 0)
        return;

    glBindVertexArray(VertexArrayID);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0);
}
