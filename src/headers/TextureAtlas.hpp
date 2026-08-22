#pragma once

#include <glad/glad.h>

// Loads the block atlas as an array texture: one layer per tile, rather than
// one big image with the tiles laid out in a grid.
//
// This is what makes mipmapping possible. Every block samples a single 16x16
// tile of a shared 256x256 image, and the lower mip levels of that image average
// across tile boundaries -- so a solid block picks up a seam of whatever happens
// to sit next to it in the atlas, and the see-through plants come off worse
// still, because averaging their transparent texels in drags the alpha under the
// cut-out threshold and eats the plant away with distance. That is why mipmaps
// were switched off entirely, and why everything shimmered instead.
//
// An array texture has no neighbours to bleed from: each layer has its own mip
// chain, built only from itself. The chain is built here rather than by
// glGenerateMipmap because a plain box filter gets the cut-outs wrong, and the
// two corrections it needs are explained at the top of Texture.cpp.
//
// Returns 0 if the image could not be read.
GLuint LoadBlockAtlasArray(const char* imagePath, int tileSize);

// Anisotropic filtering, if the driver has it. Ground seen at a grazing angle is
// where nearly all of the remaining shimmer lives, because an isotropic filter
// has to pick one level of detail for a footprint that is long in one direction
// and narrow in the other, and blurs the narrow direction to suit the long one.
// An extension rather than core in 3.3, so it is asked for by name.
void EnableAnisotropicFiltering(GLenum target);
