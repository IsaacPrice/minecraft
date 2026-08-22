#version 330 core

// Chunk-local position, packed into one integer in sixteenths of a block:
// x in bits 0-8, y in bits 9-20, z in bits 21-29. See Vertex.hpp.
layout(location = 0) in uint packedPosition;

// Atlas tile in bits 0-7, the corner of that tile in bits 8 and 9, whether to
// pull one texel in on every side in bit 10, and what kind of foliage this is
// -- if any -- in bits 11 and 12.
layout(location = 1) in uint packedTexture;

layout(location = 2) in ivec2 chunkOrigin;

out vec2 UV;
flat out float tileLayer;
out float cameraDistance;

uniform mat4 MVP;
uniform vec3 cameraPosition;

// How far out, in chunks, small plants are still drawn.
uniform float foliageDistance;

// Sixteen units to a block, sixteen blocks to a chunk, one world unit to a
// chunk: so a packed unit is a two hundred and fifty sixth of a world unit.
const float UNIT = 1.0 / 256.0;

// A tile is sixteen texels across, so one texel is a sixteenth of it.
const float TEXEL = 1.0 / 16.0;

// The solid leaf tile, which the atlas carries alongside the see-through one.
// LEAVES_OPAQUE in BlockData.hpp is block 54, and a tile is its id less one.
const uint SOLID_LEAF_TILE = 53u;

void main() {
	vec3 local = vec3(float( packedPosition        & 511u),
	                  float((packedPosition >>  9u) & 4095u),
	                  float((packedPosition >> 21u) & 511u)) * UNIT;

	vec3 world = local + vec3(float(chunkOrigin.x), 0.0, float(chunkOrigin.y));

	uint tile = packedTexture & 255u;
	bool smallFoliage = ((packedTexture >> 11u) & 1u) == 1u;
	bool leaf = ((packedTexture >> 12u) & 1u) == 1u;
	bool interiorLeaf = ((packedTexture >> 13u) & 1u) == 1u;

	// Measured to the middle of the chunk rather than to this vertex, so every
	// vertex of a quad -- and every quad in a chunk -- makes the same decision.
	// A per-vertex test would cut some corners of a plant and not others and
	// leave torn geometry behind; a per-chunk one either keeps a plant whole or
	// drops it whole.
	vec2 chunkCentre = vec2(float(chunkOrigin.x), float(chunkOrigin.y)) + 0.5;
	bool beyondFoliage = distance(chunkCentre, cameraPosition.xz) > foliageDistance;

	if (beyondFoliage && leaf) {
		// Far enough away that the gaps in a canopy are smaller than a pixel,
		// so the solid tile is what it looks like anyway -- and it costs no
		// cut-out test to draw.
		tile = SOLID_LEAF_TILE;
	}

	// The faces inside a canopy are what give it depth close up, and are worth
	// nothing once it is drawn with the solid tile: the outside is opaque by
	// then, so every one of them is hidden. Dropping them there is what fast
	// graphics does, and it pays back the extra geometry fancy leaves cost.
	if (beyondFoliage && (smallFoliage || interiorLeaf)) {
		// Pushed past the far plane, where clipping throws the whole triangle
		// away before it is ever rasterised.
		gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
	} else {
		gl_Position = MVP * vec4(world, 1);
	}

	// Built here rather than stored per vertex: sixteenths are exact in floating
	// point, so a tile edge lands exactly on a tile edge. The atlas is an array
	// of tiles, so a face samples its own layer over the whole zero to one range
	// and there is no neighbouring tile to stray into.
	tileLayer = float(tile);

	vec2 corner = vec2(float((packedTexture >> 8u) & 1u),
	                   float((packedTexture >> 9u) & 1u));
	float inset = float((packedTexture >> 10u) & 1u) * TEXEL;

	UV = mix(vec2(inset), vec2(1.0 - inset), corner);

	// Fog is keyed off radial distance rather than depth so that turning the
	// camera does not slide the fog band across a stationary hillside.
	cameraDistance = length(world - cameraPosition);
}
