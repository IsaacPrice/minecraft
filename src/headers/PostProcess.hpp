#pragma once

#include <glad/glad.h>

// An offscreen colour and depth buffer for the world to be drawn into, and a
// full screen pass that resolves it to the window through FXAA.
//
// Mipmaps and anisotropic filtering deal with the aliasing inside a texture,
// which in a world made of textured cubes is most of it. What they cannot touch
// is the aliasing along a silhouette -- the stepped edge where the top of a
// hill meets the sky, or where a tree trunk crosses the grass behind it.
//
// FXAA finds those edges in the finished image by looking at luminance rather
// than at geometry, and blends across them. Being a post-process it costs the
// same regardless of how much geometry is on screen, which suits a renderer
// that is already bound by how many draw calls it makes, and unlike multisampling
// it needs no extra samples per pixel and no separate resolve of the depth buffer.
class PostProcess
{
public:
    PostProcess() = default;
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    // Builds the offscreen buffers and loads the resolve shader. Returns false
    // if either could not be created, in which case the caller should draw
    // straight to the window and skip the resolve.
    bool Create(int width, int height);

    // Points rendering at the offscreen buffer and clears it.
    void BeginScene(float red, float green, float blue) const;

    // Points rendering back at the window and draws the offscreen buffer over
    // it through the antialiasing pass.
    void Resolve() const;

    bool Ready() const { return _framebuffer != 0 && _program != 0; }

private:
    void Release();

    GLuint _framebuffer = 0;
    GLuint _colourTexture = 0;
    GLuint _depthBuffer = 0;

    // The full screen pass covers the window with a single oversized triangle,
    // whose corners the vertex shader makes up from gl_VertexID. That needs no
    // vertex buffer at all, but core profile still insists on some vertex array
    // being bound, so an empty one is kept for it.
    GLuint _emptyVertexArray = 0;

    GLuint _program = 0;
    GLint _screenTextureUniform = -1;
    GLint _inverseScreenSizeUniform = -1;

    int _width = 0;
    int _height = 0;
};
