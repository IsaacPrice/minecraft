#version 330 core

in vec2 UV;
out vec3 color;

uniform sampler2D myTextureSampler;
uniform vec3 lightDirection; // Direction of the sunlight

void main() {
    vec3 objectColor = texture(myTextureSampler, UV).rgb;
    vec3 normalizedLightDir = normalize(lightDirection);

    // Simple diffuse lighting
    float diffuse = max(dot(normalizedLightDir, vec3(0.0, 0.0, 1.0)), 0.2); // Assuming normals facing up (z-axis)

    // Combining the texture color with the lighting
    color = objectColor * diffuse * vec3(1.0, 1.0, 1.0); // Light color is white
}