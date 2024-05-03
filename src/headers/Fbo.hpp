#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

class Fbo
{
public:
    static const int NONE = 0;
    static const int DEPTH_TEXTURE = 1;
    static const int DEPTH_RENDER_BUFFER = 2;

private:
    GLFWwindow* window;

    int* width;
    int* height;

    GLuint frameBuffer;

    bool multisample;

    GLuint colorTexture;
    GLuint depthTexture;

    GLuint depthBuffer;
    GLuint colorBuffer;

    void initializeFrameBuffer(int type);
    void createFrameBuffer();
    void createTextureAttachment();
    void createDepthTextureAttachment();
    void createMultisampledColorAttachment();
    void createDepthBufferAttachment();

public:
    Fbo(GLFWwindow *window, int width, int height, int depthBufferType);
    Fbo(GLFWwindow *window, int width, int height);

    void cleanUp() const;
    void bindFrameBuffer() const;
    void unbindFrameBuffer() const;
    void bindToRead() const;
    void resolveToFbo(Fbo *outputFbo) const;
    void resolveToScreen() const;

    inline int getColorTexture() const { return colorTexture; }
    inline int getDepthTexture() const { return depthTexture; }
};