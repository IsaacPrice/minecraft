#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Vertex.hpp"

extern GLuint programID;

// Uploads the block atlas and looks up the sampler uniform. Called once after
// the shader program is linked. This used to happen lazily inside Object, which
// meant every chunk rebound the same texture and reset the same uniform on
// every draw: at a full render distance that was several thousand redundant
// state changes a frame.
void InitRenderResources();

// Binds the block atlas for the frame. Every chunk samples the same one, so it
// is bound once around the draw loop rather than once per chunk.
void BindTerrainTexture();

// Anisotropic filtering on the block atlas. A texture parameter, not part of the
// upload, so the graphics menu can change it on the atlas already in memory
// instead of decoding and re-uploading a quarter of a megabyte of tiles.
void SetTerrainAnisotropy(int level);

// The highest level worth offering in that menu, or 1 where the driver has no
// anisotropic filtering at all.
int MaxTerrainAnisotropy();

class Object {
private:
    GLuint VertexArrayID = 0;
    GLuint vertexBuffer = 0;

    GLsizei indexCount = 0;

    void release();

public:
    Object() = default;
    explicit Object(const std::vector<Vertex>& vertices);
    ~Object();

    // Owns OpenGL handles, so copying would give two objects the same names and
    // make the second destructor delete buffers the first still draws from.
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    Object(Object&& other) noexcept;
    Object& operator=(Object&& other) noexcept;

    // Takes quad corners: four to a quad, wound to match the shared index
    // buffer's 0 1 2 2 3 0.
    void Create(const std::vector<Vertex>& vertices);
    void Draw() const;

    bool Empty() const { return indexCount == 0; }

    // What Draw submits. Kept after the vertex data has been handed to the GPU
    // and dropped, which is why the debug overlay reads this rather than the
    // vertex vectors.
    GLsizei IndexCount() const { return indexCount; }
};
