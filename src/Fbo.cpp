#include "headers/Fbo.hpp"
#include <stdio.h>

Fbo::Fbo(GLFWwindow *window, int width, int height)
{
    this->width = new int(width);
    this->height = new int(height);
    this->window = window;

    multisample = true;
    initializeFrameBuffer(DEPTH_RENDER_BUFFER);
}

void Fbo::cleanUp() const
{
    glDeleteFramebuffers(1, &frameBuffer);
    glDeleteTextures(1, &colorTexture);
    glDeleteTextures(1, &depthTexture);
    glDeleteRenderbuffers(1, &depthBuffer);
    glDeleteRenderbuffers(1, &colorBuffer);
}

void Fbo::bindFrameBuffer() const
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer);
    glViewport(0, 0, *width, *height);
}

void Fbo::unbindFrameBuffer() const
{
    glfwGetWindowSize(window, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, *width, *height);
}

void Fbo::bindToRead() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
}

void Fbo::resolveToFbo(Fbo *outputFbo) const
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outputFbo->frameBuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer);
    glBlitFramebuffer(0, 0, *width, *height, 0, 0, *outputFbo->width, *outputFbo->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    unbindFrameBuffer();
}

void Fbo::resolveToScreen() const
{
    std::cout << "Resolved to Screen\n";
    glfwGetWindowSize(window, width, height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer);
    glDrawBuffer(GL_BACK);
    glBlitFramebuffer(0, 0, *width, *height, 0, 0, *width, *height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    unbindFrameBuffer();
}

void Fbo::initializeFrameBuffer(int type)
{
    createFrameBuffer();

    if (multisample)
        createMultisampledColorAttachment();
    else
        createTextureAttachment();

    if (type == DEPTH_RENDER_BUFFER)
    {
        createDepthBufferAttachment();
    }
    else if (type == DEPTH_TEXTURE)
    {
        createDepthTextureAttachment();
    }

    unbindFrameBuffer();
}

void Fbo::createFrameBuffer()
{
    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
}

void Fbo::createTextureAttachment()
{
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, *width, *height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
}

void Fbo::createDepthTextureAttachment()
{
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, *width, *height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
}

void Fbo::createMultisampledColorAttachment()
{
    glGenRenderbuffers(1, &colorBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, *width, *height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer);
}

void Fbo::createDepthBufferAttachment()
{
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);

    if (!multisample)
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, *width, *height);
    else
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, *width, *height);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
}