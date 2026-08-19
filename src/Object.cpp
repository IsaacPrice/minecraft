#include "headers/Object.hpp"

#include <cstdio>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include "headers/stb_image.h"

namespace
{
    // The block atlas is shared by every chunk, so it is uploaded once and reused.
    GLuint terrainTexture = 0;
    GLint terrainSamplerUniform = -1;

    void ensureTerrainTexture()
    {
        if (terrainTexture != 0)
            return;

        terrainTexture = loadPNG("content/terrain.png");
        terrainSamplerUniform = glGetUniformLocation(programID, "myTextureSampler");
    }
}

Object::Object(std::vector<glm::vec3>& vertex, std::vector<glm::vec2>& uvs)
{
    Create(vertex, uvs);
}

Object::~Object()
{
    release();
}

Object::Object(Object&& other) noexcept
    : VertexArrayID(other.VertexArrayID),
      vertexBuffer(other.vertexBuffer),
      uvBuffer(other.uvBuffer),
      vertices_size(other.vertices_size)
{
    other.VertexArrayID = 0;
    other.vertexBuffer = 0;
    other.uvBuffer = 0;
    other.vertices_size = 0;
}

Object& Object::operator=(Object&& other) noexcept
{
    if (this == &other)
        return *this;

    release();

    VertexArrayID = other.VertexArrayID;
    vertexBuffer = other.vertexBuffer;
    uvBuffer = other.uvBuffer;
    vertices_size = other.vertices_size;

    other.VertexArrayID = 0;
    other.vertexBuffer = 0;
    other.uvBuffer = 0;
    other.vertices_size = 0;

    return *this;
}

void Object::release()
{
    // Chunks are built on worker threads, which have no GL context, and are
    // destroyed there if the world shuts down mid-build. Those Objects never
    // had buffers generated, so returning early keeps GL calls on the render
    // thread where they belong.
    if (VertexArrayID == 0 && vertexBuffer == 0 && uvBuffer == 0)
    {
        vertices_size = 0;
        return;
    }

    glDeleteBuffers(1, &vertexBuffer);
    glDeleteBuffers(1, &uvBuffer);
    glDeleteVertexArrays(1, &VertexArrayID);

    vertexBuffer = 0;
    uvBuffer = 0;
    VertexArrayID = 0;
    vertices_size = 0;
}

void Object::Create(std::vector<glm::vec3>& vertex, std::vector<glm::vec2>& uvs)
{
    // Create() is called again when the render distance changes, so drop any
    // buffers this Object already owns rather than leaking them.
    release();

    vertices_size = static_cast<GLsizei>(vertex.size());

    if (vertex.empty() || uvs.empty())
        return;

    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(glm::vec3), &vertex[0], GL_STATIC_DRAW);

    glGenBuffers(1, &uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);

    ensureTerrainTexture();
}

void Object::Draw()
{
    if (vertices_size == 0)
        return;

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, terrainTexture);
    glUniform1i(terrainSamplerUniform, 0);

    glDrawArrays(GL_TRIANGLES, 0, vertices_size);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

GLuint loadPNG(const char* imagepath, bool useAlphaChannel)
{
    int width, height, nrChannels;
    unsigned char* data = stbi_load(imagepath, &width, &height, &nrChannels, 0);
    if (!data)
    {
        printf("%s could not be opened. Is the working directory the project root?\n", imagepath);
        return 0;
    }

    GLenum format;
    if (nrChannels == 1)
        format = GL_RED;
    else if (nrChannels == 3)
        format = useAlphaChannel ? GL_RGBA : GL_RGB;
    else if (nrChannels == 4)
        format = GL_RGBA;
    else
    {
        printf("%s has an unsupported channel count (%d).\n", imagepath, nrChannels);
        stbi_image_free(data);
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4);

    stbi_image_free(data);

    return textureID;
}
