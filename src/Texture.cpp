#include "headers/TextureAtlas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "headers/BlockData.hpp"
#include "headers/stb_image.h"

namespace
{
    // The cut-out threshold the fragment shader discards below. Everything about
    // the mip chain for a plant depends on this number matching.
    const float ALPHA_THRESHOLD = 0.5f;

    // How much of its coverage a thin plant is asked to keep at each successive
    // mip level.
    //
    // Averaging alpha spreads a plant outwards rather than shrinking it: half a
    // texel of stem and half a texel of nothing average to a texel that is half
    // opaque, and half opaque is over the cut-out. Measured on the atlas, sugar
    // cane goes from 55% of its tile covered to 100% by the first mip level --
    // a handful of stems becomes a solid slab of stem colour. Tall grass and
    // leaves climb the same way.
    //
    // Holding coverage at its original value undoes that, but only back to the
    // density the plant has up close, and that is still too much: these are
    // upright crossed quads, so from a distance they overlap into a dark carpet
    // over ground that should be showing through them. Thinning them further
    // with each level is what lets the ground back through, and it eases the
    // distance at which foliage is dropped altogether.
    //
    // Only the thin plants get this. Leaves are a canopy that is meant to read
    // as solid from any distance, and thinning them would open holes in it.
    const float THIN_PLANT_COVERAGE_FADE = 0.75f;

    // One mip level of one tile.
    struct Layer
    {
        int size = 0;
        std::vector<unsigned char> rgba;
    };

    // Halves a layer, with two corrections that a plain box filter does not make.
    //
    // The first is that colour is averaged weighted by alpha. A leaf tile is a
    // leaf shape over texels that are transparent, and those transparent texels
    // still carry some colour -- usually black. Averaging colour without regard
    // to alpha mixes that black into the leaf and every mip level comes out
    // darker than the one above it, so a canopy fades to a grey smudge with
    // distance. Weighting by alpha means a transparent texel contributes nothing
    // but its transparency.
    Layer halve(const Layer& source)
    {
        Layer result;
        result.size = std::max(1, source.size / 2);
        result.rgba.resize(static_cast<size_t>(result.size) * result.size * 4);

        for (int y = 0; y < result.size; y++)
        {
            for (int x = 0; x < result.size; x++)
            {
                float red = 0.0f, green = 0.0f, blue = 0.0f;
                float alphaSum = 0.0f, weight = 0.0f;

                for (int offsetY = 0; offsetY < 2; offsetY++)
                {
                    for (int offsetX = 0; offsetX < 2; offsetX++)
                    {
                        int sourceX = std::min(source.size - 1, x * 2 + offsetX);
                        int sourceY = std::min(source.size - 1, y * 2 + offsetY);

                        const unsigned char* texel =
                            &source.rgba[(static_cast<size_t>(sourceY) * source.size + sourceX) * 4];

                        float alpha = texel[3] / 255.0f;

                        red += texel[0] * alpha;
                        green += texel[1] * alpha;
                        blue += texel[2] * alpha;
                        alphaSum += alpha;
                        weight += alpha;
                    }
                }

                unsigned char* out = &result.rgba[(static_cast<size_t>(y) * result.size + x) * 4];

                if (weight > 0.0f)
                {
                    out[0] = static_cast<unsigned char>(red / weight + 0.5f);
                    out[1] = static_cast<unsigned char>(green / weight + 0.5f);
                    out[2] = static_cast<unsigned char>(blue / weight + 0.5f);
                }
                else
                {
                    // All four sources were transparent. Their colour still
                    // matters -- see dilateColour below -- so it is carried
                    // down as a plain average rather than blacked out.
                    int red2 = 0, green2 = 0, blue2 = 0;
                    for (int offsetY = 0; offsetY < 2; offsetY++)
                    {
                        for (int offsetX = 0; offsetX < 2; offsetX++)
                        {
                            int sourceX = std::min(source.size - 1, x * 2 + offsetX);
                            int sourceY = std::min(source.size - 1, y * 2 + offsetY);
                            const unsigned char* texel =
                                &source.rgba[(static_cast<size_t>(sourceY) * source.size + sourceX) * 4];
                            red2 += texel[0];
                            green2 += texel[1];
                            blue2 += texel[2];
                        }
                    }
                    out[0] = static_cast<unsigned char>(red2 / 4);
                    out[1] = static_cast<unsigned char>(green2 / 4);
                    out[2] = static_cast<unsigned char>(blue2 / 4);
                }

                out[3] = static_cast<unsigned char>(alphaSum / 4.0f * 255.0f + 0.5f);
            }
        }

        return result;
    }

    // Floods the colour of a tile outwards into its transparent texels.
    //
    // Every transparent texel in the atlas is stored as pure black -- all 92 of
    // them in the tall grass tile, all 115 in the sugar cane, all 226 in the
    // rose. That is invisible at the top level, because a transparent texel is
    // discarded before its colour is ever used. It stops being invisible as
    // soon as there are mip levels: the minification filter blends between two
    // of them, so a texel that is green at one level and black-and-transparent
    // at the next comes out as a darker green, and its alpha can still be over
    // the cut-out. The plant is kept and drawn, in a colour that is part
    // background black -- which is the odd darkening that shows at the middle
    // distances, where the filter sits between two levels.
    //
    // Giving those texels the colour of their opaque neighbours instead means
    // there is no black anywhere in the tile for the filter to find. Their
    // alpha is untouched, so nothing new becomes visible; only what the filter
    // mixes in changes.
    void dilateColour(Layer& layer)
    {
        const int size = layer.size;

        std::vector<unsigned char> known(static_cast<size_t>(size) * size);
        for (size_t i = 0; i < known.size(); i++)
            known[i] = layer.rgba[i * 4 + 3] > 0 ? 1 : 0;

        // Each pass grows the known region by one texel, so a tile can never
        // need more passes than it is wide.
        for (int pass = 0; pass < size; pass++)
        {
            std::vector<unsigned char> grown = known;
            bool changed = false;

            for (int y = 0; y < size; y++)
            {
                for (int x = 0; x < size; x++)
                {
                    size_t index = static_cast<size_t>(y) * size + x;
                    if (known[index])
                        continue;

                    int red = 0, green = 0, blue = 0, count = 0;

                    for (int offsetY = -1; offsetY <= 1; offsetY++)
                    {
                        for (int offsetX = -1; offsetX <= 1; offsetX++)
                        {
                            int nearX = x + offsetX;
                            int nearY = y + offsetY;
                            if (nearX < 0 || nearY < 0 || nearX >= size || nearY >= size)
                                continue;

                            size_t nearIndex = static_cast<size_t>(nearY) * size + nearX;
                            if (!known[nearIndex])
                                continue;

                            red += layer.rgba[nearIndex * 4 + 0];
                            green += layer.rgba[nearIndex * 4 + 1];
                            blue += layer.rgba[nearIndex * 4 + 2];
                            count++;
                        }
                    }

                    if (count == 0)
                        continue;

                    layer.rgba[index * 4 + 0] = static_cast<unsigned char>(red / count);
                    layer.rgba[index * 4 + 1] = static_cast<unsigned char>(green / count);
                    layer.rgba[index * 4 + 2] = static_cast<unsigned char>(blue / count);
                    grown[index] = 1;
                    changed = true;
                }
            }

            known.swap(grown);
            if (!changed)
                break;
        }
    }


    // What fraction of a layer survives the cut-out, with all its alpha scaled.
    float coverageOf(const Layer& layer, float scale)
    {
        size_t texels = static_cast<size_t>(layer.size) * layer.size;
        if (texels == 0)
            return 0.0f;

        size_t kept = 0;
        for (size_t i = 0; i < texels; i++)
        {
            if (layer.rgba[i * 4 + 3] / 255.0f * scale >= ALPHA_THRESHOLD)
                kept++;
        }

        return static_cast<float>(kept) / texels;
    }

    // The second correction. Halving a layer averages the alpha at the edge of a
    // plant with the nothing beside it, so each level has a little less of its
    // area above the cut-out than the level above -- and the plant visibly
    // shrinks and then dissolves as it recedes. Scaling the whole level's alpha
    // until the same fraction of it survives the threshold as survived at full
    // size puts the silhouette back where it belongs.
    void matchCoverage(Layer& layer, float targetCoverage)
    {
        if (targetCoverage <= 0.0f)
            return;

        // A straight bisection: coverage rises monotonically with the scale.
        float low = 0.0f;
        float high = 8.0f;
        float best = 1.0f;
        float bestError = 2.0f;

        for (int step = 0; step < 16; step++)
        {
            float middle = (low + high) * 0.5f;
            float coverage = coverageOf(layer, middle);
            float error = std::fabs(coverage - targetCoverage);

            if (error < bestError)
            {
                bestError = error;
                best = middle;
            }

            if (coverage < targetCoverage)
                low = middle;
            else
                high = middle;
        }

        for (size_t i = 3; i < layer.rgba.size(); i += 4)
        {
            float alpha = layer.rgba[i] / 255.0f * best;
            layer.rgba[i] = static_cast<unsigned char>(std::min(255.0f, alpha * 255.0f + 0.5f));
        }
    }

    bool hasExtension(const char* name)
    {
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (GLint i = 0; i < count; i++)
        {
            const GLubyte* extension = glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
            if (extension && std::strcmp(reinterpret_cast<const char*>(extension), name) == 0)
                return true;
        }

        return false;
    }
}


void EnableAnisotropicFiltering(GLenum target)
{
    // Not in glad's core 3.3 header, so the values are spelled out. They are
    // fixed by the extension and the same everywhere.
    const GLenum MAX_ANISOTROPY = 0x84FF;
    const GLenum TEXTURE_MAX_ANISOTROPY = 0x84FE;

    if (!hasExtension("GL_EXT_texture_filter_anisotropic") &&
        !hasExtension("GL_ARB_texture_filter_anisotropic"))
    {
        return;
    }

    GLfloat limit = 1.0f;
    glGetFloatv(MAX_ANISOTROPY, &limit);

    // Past about sixteen there is nothing left to recover.
    glTexParameterf(target, TEXTURE_MAX_ANISOTROPY, std::min(limit, 16.0f));
}


GLuint LoadBlockAtlasArray(const char* imagePath, int tileSize)
{
    int width = 0, height = 0, channels = 0;

    // Forced to four channels: the cut-out needs an alpha even for tiles that
    // were stored without one.
    unsigned char* image = stbi_load(imagePath, &width, &height, &channels, 4);
    if (!image)
    {
        printf("%s could not be opened. Is the working directory the project root?\n", imagePath);
        return 0;
    }

    if (width % tileSize != 0 || height % tileSize != 0)
    {
        printf("%s is %dx%d, which is not a whole number of %d pixel tiles.\n",
               imagePath, width, height, tileSize);
        stbi_image_free(image);
        return 0;
    }

    const int tilesAcross = width / tileSize;
    const int tilesDown = height / tileSize;
    const int layerCount = tilesAcross * tilesDown;

    // Cut the grid into one layer per tile, in atlas order, so a layer index is
    // the tile index the mesher already puts in every vertex.
    std::vector<Layer> layers(layerCount);
    for (int layerIndex = 0; layerIndex < layerCount; layerIndex++)
    {
        Layer& layer = layers[layerIndex];
        layer.size = tileSize;
        layer.rgba.resize(static_cast<size_t>(tileSize) * tileSize * 4);

        int originX = (layerIndex % tilesAcross) * tileSize;
        int originY = (layerIndex / tilesAcross) * tileSize;

        for (int y = 0; y < tileSize; y++)
        {
            const unsigned char* source = image + ((static_cast<size_t>(originY + y) * width + originX) * 4);
            std::memcpy(&layer.rgba[static_cast<size_t>(y) * tileSize * 4], source,
                        static_cast<size_t>(tileSize) * 4);
        }
    }

    stbi_image_free(image);

    // Before anything is measured or halved, so that no level ever holds a
    // black transparent texel for the filter to blend in.
    for (int i = 0; i < layerCount; i++)
        dilateColour(layers[i]);

    // The coverage each tile has at full size, which its mip levels are then
    // held near, and whether it is one of the thin plants that also thins out.
    //
    // A layer index is a tile index, and a tile index is a block id less one, so
    // the same isCross the mesher uses to decide what stands on the diagonals
    // answers this too.
    std::vector<float> targetCoverage(layerCount);
    std::vector<bool> thinPlant(layerCount);
    for (int i = 0; i < layerCount; i++)
    {
        targetCoverage[i] = coverageOf(layers[i], 1.0f);
        thinPlant[i] = isCross(static_cast<unsigned short>(i + 1));
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    int levelCount = 0;
    for (int size = tileSize; size >= 1; size /= 2)
        levelCount++;

    // Each level is uploaded as one block of all the layers back to back, which
    // is the memory layout glTexImage3D wants.
    std::vector<unsigned char> levelData;

    // The coverage correction is applied to the copy that is uploaded, never to
    // the one the next level is halved from.
    //
    // Feeding the corrected alpha forward was what made foliage turn dark and
    // heavy a couple of levels down: raising a level's alpha to restore its
    // coverage also widens every blade in it, and halving that already widened
    // level and widening it again compounds. Two levels in, a plant that should
    // have been thinning into scattered specks had spread into a solid block of
    // the darkest colour in its tile.
    Layer scaled;

    for (int level = 0; level < levelCount; level++)
    {
        int levelSize = layers[0].size;
        float thinFade = std::pow(THIN_PLANT_COVERAGE_FADE, static_cast<float>(level));

        levelData.resize(static_cast<size_t>(levelSize) * levelSize * 4 * layerCount);
        for (int i = 0; i < layerCount; i++)
        {
            scaled = layers[i];

            // Level zero is the original and already has the coverage the rest
            // are measured against.
            if (level > 0)
                matchCoverage(scaled, targetCoverage[i] * (thinPlant[i] ? thinFade : 1.0f));

            std::memcpy(&levelData[static_cast<size_t>(i) * levelSize * levelSize * 4],
                        scaled.rgba.data(), scaled.rgba.size());
        }

        glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA8, levelSize, levelSize, layerCount,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, levelData.data());

        if (level + 1 == levelCount)
            break;

        for (int i = 0; i < layerCount; i++)
        {
            layers[i] = halve(layers[i]);
            dilateColour(layers[i]);
        }
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, levelCount - 1);

    // Nearest magnification keeps blocks blocky up close, which is the whole
    // look. Minification takes the nearest texel within a level but blends
    // between levels, which is what Minecraft itself does: it removes the
    // shimmer without softening the texture into mush.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);

    // Clamped, not repeated: a block face maps to exactly one tile, so a
    // coordinate that lands on 1.0 should stay on the last texel of this layer
    // rather than wrapping around to the first.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    EnableAnisotropicFiltering(GL_TEXTURE_2D_ARRAY);

    return texture;
}
