#version 330 core

in vec2 UV;
out vec4 color;

uniform sampler2D myTextureSampler;
uniform vec3 lightDirection; // Direction of the sunlight
uniform float alphaScale;    // 1.0 for the solid pass, lower for water

void main() {
    vec4 texel = texture(myTextureSampler, UV);

    // Plants and leaves are cut out of a square tile, so the transparent part
    // of the tile has to be thrown away rather than blended. Discarding keeps
    // depth writes correct without needing the chunks sorted back to front.
    if (texel.a < 0.5)
        discard;

    vec3 normalizedLightDir = normalize(lightDirection);

    // Simple diffuse lighting
    float diffuse = max(dot(normalizedLightDir, vec3(0.0, 0.0, 1.0)), 0.2); // Assuming normals facing up (z-axis)

    // Combining the texture color with the lighting
    color = vec4(texel.rgb * diffuse, texel.a * alphaScale);
}
