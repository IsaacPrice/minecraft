#version 330 core

in vec2 UV;
in vec4 tint;

out vec4 color;

uniform sampler2D uiTexture;

void main() {
	// The font atlas is white throughout, with the glyph carried entirely in the
	// alpha channel. So a straight multiply tints text to any colour, and a quad
	// pointed at the solid white cell of the atlas comes out as flat colour --
	// which is how panels, slider tracks and the dim behind the pause menu are
	// drawn through the same batch as the text, with no second texture and no
	// branch in here to pick between them.
	color = texture(uiTexture, UV) * tint;
}
