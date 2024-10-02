#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

using namespace std;
using namespace glm;

extern GLuint programID;
extern GLuint Texture;
extern GLuint TextureID;

GLuint loadPNG(const char *imagepath, bool useAlphaChannel = false);

class Object {
private:
    GLuint VertexArrayID;
    GLuint vertexBuffer;
    GLuint uvBuffer;

    uint vertices_size;

public:
    Object();
    Object(vector<vec3> &vertex, vector<vec2> &uvs); 
    ~Object();

    void Create(vector<vec3> &vertex, vector<vec2> &uvs);
    void Draw();
};
