#pragma once

#include <string>
#include <vector>

#include <glad/glad.h>

// A colour as four bytes, which is what the vertex carries. Sixteen bytes a
// vertex would be four times the size for a value that only ever comes from a
// small palette of flat menu colours.
struct Colour
{
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

inline Colour rgba(int r, int g, int b, int a = 255)
{
    Colour colour;
    colour.r = static_cast<unsigned char>(r);
    colour.g = static_cast<unsigned char>(g);
    colour.b = static_cast<unsigned char>(b);
    colour.a = static_cast<unsigned char>(a);
    return colour;
}

// Draws the menus: flat rectangles and bitmap text, batched into one buffer and
// submitted as a single draw call for the whole frame.
//
// This is deliberately not built on the Object/Vertex pair the world uses. That
// vertex packs a chunk-local block coordinate and an atlas tile into twelve
// bytes, which is exactly right for several million of them a frame and no use
// at all for a few hundred screen-space quads that need arbitrary positions and
// a per-corner colour.
//
// Where this draws in the frame matters and is easy to get wrong. It has to come
// after PostProcess::Resolve, into the window rather than the offscreen buffer,
// for two separate reasons: FXAA is a luminance edge filter and would soften
// every glyph edge, which is the one place in this renderer where a hard pixel
// edge is the entire point; and anything drawn inside the world pass is fogged
// and depth-tested against terrain, so a menu would haze out and be buried by
// the nearest hillside.
class UIRenderer
{
public:
    UIRenderer() = default;
    ~UIRenderer();

    UIRenderer(const UIRenderer&) = delete;
    UIRenderer& operator=(const UIRenderer&) = delete;

    // Loads the shader and the font. False if either failed, in which case the
    // caller should carry on without a UI rather than refuse to start.
    bool Create();

    bool Ready() const { return _program != 0 && _fontTexture != 0; }

    // Sets up the projection for this window size and starts a fresh batch.
    void BeginFrame(int windowWidth, int windowHeight);

    // Uploads and draws everything queued since BeginFrame.
    void EndFrame();

    void DrawRect(float x, float y, float w, float h, Colour colour);

    // Four rectangles rather than a rectangle with a hole, because a hole would
    // need either a stencil or a second draw with a scissor. Borders here are a
    // couple of units wide, so four thin quads is the cheaper answer.
    void DrawRectOutline(float x, float y, float w, float h, float thickness, Colour colour);

    void DrawText(float x, float y, float scale, Colour colour, const std::string& text);
    void DrawTextCentred(float centreX, float y, float scale, Colour colour, const std::string& text);

    // A drop shadow one texel down and right, then the text over it. Menu text
    // sits over live terrain, which can be any colour from dark stone to bright
    // sky, and plain white on its own disappears into the pale half of that.
    void DrawTextShadowed(float x, float y, float scale, Colour colour, const std::string& text);
    void DrawTextCentredShadowed(float centreX, float y, float scale, Colour colour, const std::string& text);

    float MeasureText(const std::string& text, float scale) const;
    float LineHeight(float scale) const;

    // The canvas the menus lay themselves out on, in UI units. The height is
    // fixed; the width follows the window's aspect ratio.
    float Width() const { return _uiWidth; }
    float Height() const { return _uiHeight; }

    // Window pixels, as the mouse callbacks report them, into UI units.
    void CursorToUI(double pixelX, double pixelY, float& x, float& y) const;

private:
    struct UIVertex
    {
        float x, y;
        float u, v;
        Colour colour;
    };

    void PushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1, Colour colour);

    void Release();

    std::vector<UIVertex> _vertices;

    GLuint _vertexArray = 0;
    GLuint _vertexBuffer = 0;
    size_t _bufferCapacity = 0;

    GLuint _program = 0;
    GLint _projectionUniform = -1;
    GLint _textureUniform = -1;

    GLuint _fontTexture = 0;
    int _fontImageSize = 0;

    // Per-character advance, in glyph texels, measured off the atlas at load
    // rather than kept in a table here. Swapping content/font.png for a
    // different one then needs no code change, and there is no table to drift
    // out of step with the image.
    float _advance[256];

    float _uiWidth = 0.0f;
    float _uiHeight = 0.0f;

    // UI units per window pixel. The projection is a uniform scale, so hit
    // testing inverts it with this one number rather than rederiving it.
    float _unitsPerPixel = 1.0f;
};
