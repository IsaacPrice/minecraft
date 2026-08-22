#version 330 core

out vec2 texCoord;

// One triangle big enough to cover the window, with its corners made up from
// the vertex index. A quad would need a vertex buffer and would seam down the
// diagonal where its two triangles meet; this needs neither.
void main() {
	vec2 corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
	vec2 corner = corners[gl_VertexID];

	texCoord = (corner + 1.0) * 0.5;
	gl_Position = vec4(corner, 0.0, 1.0);
}
