#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

extern GLuint programID;

GLuint loadPNG(const char* imagepath, bool useAlphaChannel = false);

class Object {
private:
    GLuint VertexArrayID = 0;
    GLuint vertexBuffer = 0;
    GLuint uvBuffer = 0;

    GLsizei vertices_size = 0;

    void release();

public:
    Object() = default;
    Object(std::vector<glm::vec3>& vertex, std::vector<glm::vec2>& uvs);
    ~Object();

    // Owns OpenGL handles, so copying would give two objects the same names and
    // make the second destructor delete buffers the first still draws from.
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    Object(Object&& other) noexcept;
    Object& operator=(Object&& other) noexcept;

    void Create(std::vector<glm::vec3>& vertex, std::vector<glm::vec2>& uvs);
    void Draw();
};
