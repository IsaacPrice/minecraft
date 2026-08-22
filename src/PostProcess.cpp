#include "headers/PostProcess.hpp"

#include <cstdio>

#include "headers/Shader.hpp"

PostProcess::~PostProcess()
{
    Release();
}

void PostProcess::Release()
{
    if (_colourTexture) glDeleteTextures(1, &_colourTexture);
    if (_depthBuffer) glDeleteRenderbuffers(1, &_depthBuffer);
    if (_framebuffer) glDeleteFramebuffers(1, &_framebuffer);
    if (_emptyVertexArray) glDeleteVertexArrays(1, &_emptyVertexArray);
    if (_program) glDeleteProgram(_program);

    _colourTexture = 0;
    _depthBuffer = 0;
    _framebuffer = 0;
    _emptyVertexArray = 0;
    _program = 0;
}

bool PostProcess::Create(int width, int height)
{
    Release();

    _width = width;
    _height = height;

    glGenFramebuffers(1, &_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

    glGenTextures(1, &_colourTexture);
    glBindTexture(GL_TEXTURE_2D, _colourTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // The resolve reads this with linear filtering, because FXAA's whole method
    // is to sample at a fraction of a pixel across an edge and let the hardware
    // blend the two sides.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colourTexture, 0);

    // Depth only needs to exist for the world pass, and is never read
    // afterwards, so a renderbuffer rather than a texture.
    glGenRenderbuffers(1, &_depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Offscreen buffer is incomplete (0x%x); drawing without antialiasing.\n", status);
        Release();
        return false;
    }

    glGenVertexArrays(1, &_emptyVertexArray);

    _program = LoadShaders("src/shaders/post.vert", "src/shaders/post.frag");
    if (_program == 0)
    {
        Release();
        return false;
    }

    _screenTextureUniform = glGetUniformLocation(_program, "screenTexture");
    _inverseScreenSizeUniform = glGetUniformLocation(_program, "inverseScreenSize");

    return true;
}

void PostProcess::BeginScene(float red, float green, float blue) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glViewport(0, 0, _width, _height);
    glClearColor(red, green, blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcess::Resolve() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, _width, _height);

    // Nothing behind this pass and nothing after it, so neither test is wanted:
    // the triangle covers every pixel and each one is written exactly once.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _colourTexture);
    glUniform1i(_screenTextureUniform, 0);
    glUniform2f(_inverseScreenSizeUniform, 1.0f / _width, 1.0f / _height);

    glBindVertexArray(_emptyVertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}
