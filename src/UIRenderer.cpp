#include "headers/UIRenderer.hpp"

#include <cstddef>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "headers/Shader.hpp"
#include "headers/stb_image.h"

namespace
{
    // content/font.png is a 16x16 grid of cells, so a character's code point is
    // its cell: the row is the top four bits and the column the bottom four.
    const int FONT_GRID = 16;

    // Cell 0 is left solid white by the generator. Flat colour quads sample a
    // texel out of the middle of it, which is what lets them share a draw call
    // with the text instead of needing a texture swap or a shader branch.
    const int SOLID_CELL = 0;

    // The canvas the menus are laid out on is this many units tall, whatever the
    // window is. At 1080p that works out to exactly two pixels per unit, so a
    // glyph texel lands on a whole number of pixels and the font stays crisp.
    const float UI_REFERENCE_HEIGHT = 540.0f;

    // A glyph with no ink -- the space -- has no right edge to measure, so its
    // width is set rather than found.
    const float SPACE_ADVANCE = 3.0f;

    // Between one glyph and the next, in glyph texels.
    const float LETTER_SPACING = 1.0f;
}

UIRenderer::~UIRenderer()
{
    Release();
}

void UIRenderer::Release()
{
    if (_vertexBuffer) glDeleteBuffers(1, &_vertexBuffer);
    if (_vertexArray) glDeleteVertexArrays(1, &_vertexArray);
    if (_fontTexture) glDeleteTextures(1, &_fontTexture);
    if (_program) glDeleteProgram(_program);

    _vertexBuffer = 0;
    _vertexArray = 0;
    _fontTexture = 0;
    _program = 0;
    _bufferCapacity = 0;
}

bool UIRenderer::Create()
{
    Release();

    _program = LoadShaders("src/shaders/ui.vert", "src/shaders/ui.frag");
    if (_program == 0)
        return false;

    _projectionUniform = glGetUniformLocation(_program, "projection");
    _textureUniform = glGetUniformLocation(_program, "uiTexture");

    // Loaded here rather than through the block atlas path, because the two want
    // opposite things. A block tile is minified into the distance and needs the
    // hand-built mip chain and the anisotropic filtering that Texture.cpp exists
    // for; a glyph is drawn at a whole-number scale straight at the screen, so
    // there is nothing to minify and a filtered edge would only turn the letter
    // grey. The decoded pixels are also wanted here for the advance widths.
    int imageWidth = 0, imageHeight = 0, channels = 0;
    unsigned char* image = stbi_load("content/font.png", &imageWidth, &imageHeight, &channels, 4);
    if (!image)
    {
        printf("content/font.png could not be opened; the menus will not be drawn.\n");
        Release();
        return false;
    }

    if (imageWidth != imageHeight || imageWidth % FONT_GRID != 0)
    {
        printf("content/font.png must be square and a multiple of %d; it is %dx%d.\n",
               FONT_GRID, imageWidth, imageHeight);
        stbi_image_free(image);
        Release();
        return false;
    }

    _fontImageSize = imageWidth;
    const int cell = imageWidth / FONT_GRID;

    // Measured off the image rather than declared in a table: the rightmost
    // column of a cell that has any opaque texel in it is the glyph's right
    // edge, and one space past that is where the next glyph starts. A table
    // would have to be kept in step with the image by hand, and would be wrong
    // the moment anyone redrew a letter.
    for (int code = 0; code < 256; code++)
    {
        const int cellX = (code % FONT_GRID) * cell;
        const int cellY = (code / FONT_GRID) * cell;

        int rightmost = -1;
        for (int x = 0; x < cell; x++)
        {
            for (int y = 0; y < cell; y++)
            {
                const size_t index = (static_cast<size_t>(cellY + y) * imageWidth + (cellX + x)) * 4 + 3;
                if (image[index] != 0)
                {
                    rightmost = x;
                    break;
                }
            }
        }

        _advance[code] = (rightmost < 0) ? SPACE_ADVANCE : (rightmost + 1 + LETTER_SPACING);
    }

    glGenTextures(1, &_fontTexture);
    glBindTexture(GL_TEXTURE_2D, _fontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageWidth, imageHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(image);

    glGenVertexArrays(1, &_vertexArray);
    glGenBuffers(1, &_vertexBuffer);

    glBindVertexArray(_vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                          (void*)offsetof(UIVertex, x));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex),
                          (void*)offsetof(UIVertex, u));

    // Normalised, so the shader sees a 0-1 colour without the bytes having to be
    // widened to floats on the way into the buffer.
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UIVertex),
                          (void*)offsetof(UIVertex, colour));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void UIRenderer::BeginFrame(int windowWidth, int windowHeight)
{
    _uiHeight = UI_REFERENCE_HEIGHT;
    _uiWidth = UI_REFERENCE_HEIGHT * (float)windowWidth / (float)windowHeight;
    _unitsPerPixel = UI_REFERENCE_HEIGHT / (float)windowHeight;

    _vertices.clear();
}

void UIRenderer::CursorToUI(double pixelX, double pixelY, float& x, float& y) const
{
    // Both axes share a scale, because the canvas keeps the window's aspect
    // ratio rather than stretching to it. Inverting the same number the
    // projection was built from is what stops hit testing drifting away from
    // what was actually drawn.
    x = (float)pixelX * _unitsPerPixel;
    y = (float)pixelY * _unitsPerPixel;
}

void UIRenderer::PushQuad(float x, float y, float w, float h,
                          float u0, float v0, float u1, float v1, Colour colour)
{
    UIVertex topLeft     = { x,     y,     u0, v0, colour };
    UIVertex topRight    = { x + w, y,     u1, v0, colour };
    UIVertex bottomRight = { x + w, y + h, u1, v1, colour };
    UIVertex bottomLeft  = { x,     y + h, u0, v1, colour };

    // Two triangles rather than a quad: core profile has no GL_QUADS, and at a
    // few hundred quads a frame an index buffer would save less than it costs to
    // maintain one.
    _vertices.push_back(topLeft);
    _vertices.push_back(topRight);
    _vertices.push_back(bottomRight);
    _vertices.push_back(bottomRight);
    _vertices.push_back(bottomLeft);
    _vertices.push_back(topLeft);
}

void UIRenderer::DrawRect(float x, float y, float w, float h, Colour colour)
{
    if (_fontImageSize == 0)
        return;

    // The middle of the solid cell, with all four corners on the same texel, so
    // no amount of scaling can let a neighbouring glyph bleed in.
    const float cell = 1.0f / (float)FONT_GRID;
    const float centreU = ((SOLID_CELL % FONT_GRID) + 0.5f) * cell;
    const float centreV = ((SOLID_CELL / FONT_GRID) + 0.5f) * cell;

    PushQuad(x, y, w, h, centreU, centreV, centreU, centreV, colour);
}

void UIRenderer::DrawRectOutline(float x, float y, float w, float h, float thickness, Colour colour)
{
    DrawRect(x, y, w, thickness, colour);                          // top
    DrawRect(x, y + h - thickness, w, thickness, colour);          // bottom
    DrawRect(x, y, thickness, h, colour);                          // left
    DrawRect(x + w - thickness, y, thickness, h, colour);          // right
}

void UIRenderer::DrawText(float x, float y, float scale, Colour colour, const std::string& text)
{
    if (_fontImageSize == 0)
        return;

    const int cellTexels = _fontImageSize / FONT_GRID;
    const float cellUV = 1.0f / (float)FONT_GRID;
    const float size = cellTexels * scale;

    float penX = x;

    for (size_t i = 0; i < text.size(); i++)
    {
        const unsigned char code = static_cast<unsigned char>(text[i]);

        // Drawn whole-cell and advanced by the measured width, rather than
        // drawn at the measured width. The cell is what the glyph was laid out
        // in, so cutting it short would clip anything that leans past its own
        // advance.
        const float u0 = (code % FONT_GRID) * cellUV;
        const float v0 = (code / FONT_GRID) * cellUV;

        if (code != ' ')
            PushQuad(penX, y, size, size, u0, v0, u0 + cellUV, v0 + cellUV, colour);

        penX += _advance[code] * scale;
    }
}

void UIRenderer::DrawTextCentred(float centreX, float y, float scale, Colour colour, const std::string& text)
{
    DrawText(centreX - MeasureText(text, scale) * 0.5f, y, scale, colour, text);
}

void UIRenderer::DrawTextShadowed(float x, float y, float scale, Colour colour, const std::string& text)
{
    const Colour shadow = rgba(0, 0, 0, (int)(colour.a * 0.7f));

    DrawText(x + scale, y + scale, scale, shadow, text);
    DrawText(x, y, scale, colour, text);
}

void UIRenderer::DrawTextCentredShadowed(float centreX, float y, float scale, Colour colour, const std::string& text)
{
    DrawTextShadowed(centreX - MeasureText(text, scale) * 0.5f, y, scale, colour, text);
}

float UIRenderer::MeasureText(const std::string& text, float scale) const
{
    float total = 0.0f;
    for (size_t i = 0; i < text.size(); i++)
        total += _advance[static_cast<unsigned char>(text[i])] * scale;

    // The trailing gap after the last glyph is not part of the text.
    if (!text.empty())
        total -= LETTER_SPACING * scale;

    return total;
}

float UIRenderer::LineHeight(float scale) const
{
    if (_fontImageSize == 0)
        return 0.0f;

    return (_fontImageSize / FONT_GRID) * scale;
}

void UIRenderer::EndFrame()
{
    if (_vertices.empty() || _program == 0)
        return;

    // Straight to the window, over whatever the world pass left there. Depth is
    // off because the UI has no depth of its own and the buffer still holds the
    // terrain; blending is on for the dim overlay and the cut-out glyphs.
    //
    // Culling has to come off as well. A menu quad has no meaningful facing, and
    // the projection below puts y down the screen -- which flips the handedness,
    // so quads wound the obvious way come out back-facing and the whole menu is
    // culled away. That is exactly what happened when back face culling was
    // turned on for the terrain: every click still landed, because hit testing
    // never asked the GPU anything, and nothing was drawn at all.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(_program);

    // Y down, which is how a menu is laid out and how the mouse is reported, so
    // neither has to be flipped anywhere else.
    const glm::mat4 projection = glm::ortho(0.0f, _uiWidth, _uiHeight, 0.0f);
    glUniformMatrix4fv(_projectionUniform, 1, GL_FALSE, glm::value_ptr(projection));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _fontTexture);
    glUniform1i(_textureUniform, 0);

    glBindVertexArray(_vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);

    const size_t bytes = _vertices.size() * sizeof(UIVertex);

    // Grown rather than reallocated every frame, and orphaned when it is reused,
    // so the driver can hand back fresh storage instead of waiting for the last
    // frame's draw to finish reading the old.
    if (_vertices.size() > _bufferCapacity)
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, _vertices.data(), GL_STREAM_DRAW);
        _bufferCapacity = _vertices.size();
    }
    else
    {
        glBufferData(GL_ARRAY_BUFFER, _bufferCapacity * sizeof(UIVertex), NULL, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, _vertices.data());
    }

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(_vertices.size()));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
