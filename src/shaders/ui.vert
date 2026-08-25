#version 330 core

// Screen space, in the UI's own units rather than pixels. The projection below
// maps those onto the window, which is what lets the menu be laid out once and
// come out the same size whatever the window is.
layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec4 vertexColour;

out vec2 UV;
out vec4 tint;

uniform mat4 projection;

void main() {
	gl_Position = projection * vec4(vertexPosition, 0.0, 1.0);
	UV = vertexUV;
	tint = vertexColour;
}
