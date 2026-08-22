#version 330 core

in vec2 texCoord;
out vec4 fragColor;

uniform sampler2D screenTexture;
uniform vec2 inverseScreenSize;

// FXAA: find the edges in the finished image by looking at how brightness
// changes from pixel to pixel, work out which way each edge runs and how far it
// extends, then take one bilinear sample nudged across it. That single offset
// sample is the antialiasing -- the hardware blends the two sides of the edge in
// roughly the proportion the pixel should have been covered by each.
//
// Mipmaps and anisotropic filtering already deal with the aliasing inside a
// texture, which in a world of textured cubes is most of it. What is left is the
// stepped silhouette where a hill meets the sky, and that is what this smooths.

// Below the first, an edge is too faint to be worth touching. The second scales
// that with how bright the area is, so dark corners are not over-processed.
const float EDGE_THRESHOLD_MIN = 0.0312;
const float EDGE_THRESHOLD_MAX = 0.125;

// How much of the correction for thin features -- a single bright pixel that no
// edge walk will ever find the ends of -- to allow. Too high softens the sharp
// texel edges that make the blocks look like blocks.
const float SUBPIXEL_QUALITY = 0.75;

const int EDGE_SEARCH_STEPS = 12;

// Perceived brightness. The square root keeps the comparison closer to how the
// eye weights a difference, which is what decides whether an edge is found.
float luma(vec3 colour) {
	return sqrt(dot(colour, vec3(0.299, 0.587, 0.114)));
}

// The walk along an edge starts in single pixel steps and lengthens once it is
// clear the edge runs a long way, so a long edge is still found in few samples.
float searchStep(int step) {
	if (step < 5) return 1.0;
	if (step < 6) return 1.5;
	if (step < 10) return 2.0;
	if (step < 11) return 4.0;
	return 8.0;
}

void main() {
	vec3 centreColour = texture(screenTexture, texCoord).rgb;

	float lumaCentre = luma(centreColour);
	float lumaDown  = luma(textureOffset(screenTexture, texCoord, ivec2( 0, -1)).rgb);
	float lumaUp    = luma(textureOffset(screenTexture, texCoord, ivec2( 0,  1)).rgb);
	float lumaLeft  = luma(textureOffset(screenTexture, texCoord, ivec2(-1,  0)).rgb);
	float lumaRight = luma(textureOffset(screenTexture, texCoord, ivec2( 1,  0)).rgb);

	float lumaMin = min(lumaCentre, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
	float lumaMax = max(lumaCentre, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
	float lumaRange = lumaMax - lumaMin;

	// Flat enough to leave alone. Most of the screen takes this path.
	if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX)) {
		fragColor = vec4(centreColour, 1.0);
		return;
	}

	float lumaDownLeft  = luma(textureOffset(screenTexture, texCoord, ivec2(-1, -1)).rgb);
	float lumaUpRight   = luma(textureOffset(screenTexture, texCoord, ivec2( 1,  1)).rgb);
	float lumaUpLeft    = luma(textureOffset(screenTexture, texCoord, ivec2(-1,  1)).rgb);
	float lumaDownRight = luma(textureOffset(screenTexture, texCoord, ivec2( 1, -1)).rgb);

	float lumaDownUp = lumaDown + lumaUp;
	float lumaLeftRight = lumaLeft + lumaRight;

	float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
	float lumaRightCorners = lumaDownRight + lumaUpRight;
	float lumaUpCorners = lumaUpRight + lumaUpLeft;
	float lumaDownCorners = lumaDownLeft + lumaDownRight;

	// Which way the edge runs: compare how sharply brightness bends across the
	// row against how sharply it bends down the column.
	float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners)
	                     + abs(-2.0 * lumaCentre + lumaDownUp) * 2.0
	                     + abs(-2.0 * lumaRight + lumaRightCorners);
	float edgeVertical   = abs(-2.0 * lumaUp + lumaUpCorners)
	                     + abs(-2.0 * lumaCentre + lumaLeftRight) * 2.0
	                     + abs(-2.0 * lumaDown + lumaDownCorners);

	bool isHorizontal = edgeHorizontal >= edgeVertical;

	// The two neighbours across the edge, and which side it actually lies on.
	float lumaSideA = isHorizontal ? lumaDown : lumaLeft;
	float lumaSideB = isHorizontal ? lumaUp : lumaRight;
	float gradientA = lumaSideA - lumaCentre;
	float gradientB = lumaSideB - lumaCentre;

	bool steeperTowardsA = abs(gradientA) >= abs(gradientB);
	float gradientScaled = 0.25 * max(abs(gradientA), abs(gradientB));

	float stepLength = isHorizontal ? inverseScreenSize.y : inverseScreenSize.x;
	float lumaLocalAverage;

	if (steeperTowardsA) {
		stepLength = -stepLength;
		lumaLocalAverage = 0.5 * (lumaSideA + lumaCentre);
	} else {
		lumaLocalAverage = 0.5 * (lumaSideB + lumaCentre);
	}

	// Sit exactly on the edge, then walk both ways along it until the brightness
	// stops matching -- those are its ends.
	vec2 edgeUv = texCoord;
	if (isHorizontal) edgeUv.y += stepLength * 0.5;
	else edgeUv.x += stepLength * 0.5;

	vec2 alongEdge = isHorizontal ? vec2(inverseScreenSize.x, 0.0)
	                              : vec2(0.0, inverseScreenSize.y);

	vec2 uvA = edgeUv - alongEdge;
	vec2 uvB = edgeUv + alongEdge;

	float lumaEndA = luma(texture(screenTexture, uvA).rgb) - lumaLocalAverage;
	float lumaEndB = luma(texture(screenTexture, uvB).rgb) - lumaLocalAverage;

	bool foundEndA = abs(lumaEndA) >= gradientScaled;
	bool foundEndB = abs(lumaEndB) >= gradientScaled;

	if (!foundEndA) uvA -= alongEdge;
	if (!foundEndB) uvB += alongEdge;

	if (!(foundEndA && foundEndB)) {
		for (int i = 2; i < EDGE_SEARCH_STEPS; i++) {
			if (!foundEndA) {
				lumaEndA = luma(texture(screenTexture, uvA).rgb) - lumaLocalAverage;
				foundEndA = abs(lumaEndA) >= gradientScaled;
			}
			if (!foundEndB) {
				lumaEndB = luma(texture(screenTexture, uvB).rgb) - lumaLocalAverage;
				foundEndB = abs(lumaEndB) >= gradientScaled;
			}

			if (!foundEndA) uvA -= alongEdge * searchStep(i);
			if (!foundEndB) uvB += alongEdge * searchStep(i);

			if (foundEndA && foundEndB) break;
		}
	}

	// How far along the edge this pixel sits decides how much of the other side
	// it should have been covered by.
	float distanceA = isHorizontal ? (texCoord.x - uvA.x) : (texCoord.y - uvA.y);
	float distanceB = isHorizontal ? (uvB.x - texCoord.x) : (uvB.y - texCoord.y);

	bool nearerEndA = distanceA < distanceB;
	float edgeLength = distanceA + distanceB;
	float pixelOffset = -min(distanceA, distanceB) / edgeLength + 0.5;

	// Only blend when this pixel is on the side of the edge the brightness says
	// it is. Without the check, edges get smeared the wrong way.
	bool centreIsDarker = lumaCentre < lumaLocalAverage;
	bool correctVariation = ((nearerEndA ? lumaEndA : lumaEndB) < 0.0) != centreIsDarker;

	float finalOffset = correctVariation ? pixelOffset : 0.0;

	// A lone pixel out of step with its neighbours has no edge to walk, so it is
	// handled by how far it sits from the average of the ring around it.
	float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight)
	                                    + lumaLeftCorners + lumaRightCorners);
	float subPixelAmount = clamp(abs(lumaAverage - lumaCentre) / lumaRange, 0.0, 1.0);
	float subPixelSmooth = (-2.0 * subPixelAmount + 3.0) * subPixelAmount * subPixelAmount;
	float subPixelOffset = subPixelSmooth * subPixelSmooth * SUBPIXEL_QUALITY;

	finalOffset = max(finalOffset, subPixelOffset);

	vec2 finalUv = texCoord;
	if (isHorizontal) finalUv.y += finalOffset * stepLength;
	else finalUv.x += finalOffset * stepLength;

	fragColor = vec4(texture(screenTexture, finalUv).rgb, 1.0);
}
