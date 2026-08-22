#version 330 core

in vec2 UV;
flat in float tileLayer;
in float cameraDistance;

out vec4 color;

uniform sampler2DArray myTextureSampler;
uniform vec3 lightDirection; // Direction of the sunlight
uniform float alphaScale;    // 1.0 for the solid pass, lower for water

uniform vec3 fogColor;       // The sky, so the two meet without a seam
uniform float fogStart;      // Distance at which haze begins
uniform float fogEnd;        // Distance at which only sky is left

void main() {
    vec4 texel = texture(myTextureSampler, vec3(UV, tileLayer));

    // Plants and leaves are cut out of a square tile, so the transparent part
    // of the tile has to be thrown away rather than blended. Discarding keeps
    // depth writes correct without needing the chunks sorted back to front.
    if (texel.a < 0.5)
        discard;

    vec3 normalizedLightDir = normalize(lightDirection);

    // Simple diffuse lighting
    float diffuse = max(dot(normalizedLightDir, vec3(0.0, 0.0, 1.0)), 0.2); // Assuming normals facing up (z-axis)

    vec3 lit = texel.rgb * diffuse;

    // Terrain fades into the sky over the last stretch of the render distance.
    // Without this the loaded region ends against the clear colour on a hard
    // line, and every chunk that streamed in popped into existence on it.
    float fog = clamp((cameraDistance - fogStart) / max(fogEnd - fogStart, 0.001), 0.0, 1.0);

    color = vec4(mix(lit, fogColor, fog), texel.a * alphaScale);
}
