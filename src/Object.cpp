#include "headers/Object.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "headers/stb_image.h"

GLuint Texture = 0;
GLuint TextureID = 0;

Object::Object() {};

Object::Object(vector<vec3> &vertex, vector<vec2> &uvs) 
{
    vertices_size = vertex.size();

    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(vec3), &vertex[0], GL_STATIC_DRAW);

    glGenBuffers(1, &uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(vec2), &uvs[0], GL_STATIC_DRAW);

    if (!Texture)
    { 
        Texture = loadPNG("content/terrain.png");
        TextureID = glGetUniformLocation(programID, "myTextureSampler");
        cout << "Texture: " << Texture << endl << "TextureID: " << TextureID << endl;
    }
};  

Object::~Object() 
{
    glDeleteBuffers(1, &vertexBuffer);
    glDeleteBuffers(1, &uvBuffer);
    glDeleteVertexArrays(1, &VertexArrayID);
};

void Object::Create(vector<vec3> &vertex, vector<vec2> &uvs) 
{
    vertices_size = vertex.size();

    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(vec3), &vertex[0], GL_STATIC_DRAW);

    glGenBuffers(1, &uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(vec2), &uvs[0], GL_STATIC_DRAW);

    Texture = loadPNG("content/terrain.png");
    TextureID = glGetUniformLocation(programID, "myTextureSampler");
}

void Object::Draw() 
{
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, uvBuffer);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Texture);
    glUniform1i(TextureID, 0);

    glDrawArrays(GL_TRIANGLES, 0, vertices_size);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

GLuint loadPNG(const char *imagepath, bool useAlphaChannel) 
{
    int width, height, nrChannels;
    unsigned char *data = stbi_load(imagepath, &width, &height, &nrChannels, 0);
    if (!data) 
    {
        printf("%s could not be opened. Are you in the right directory? \n", imagepath);
        getchar();
        return 0;
    }

    GLenum format;
    if (nrChannels == 1)
        format = GL_RED;
    else if (nrChannels == 3)
        format = useAlphaChannel ? GL_RGBA : GL_RGB;
    else if (nrChannels == 4)
        format = GL_RGBA;

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
