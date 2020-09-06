#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec3 fragColor;

// normalized device coordinates, not framebuffer coords
vec2 positions[3] = vec2[](
	vec2( 0.0, -0.5),
	vec2( 0.5,  0.5),
	vec2(-0.5,  0.5)
);

// R, G, B
vec3 colors[3] = vec3[](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); // division by 1.0 to transform clip coords to normalized device coords means we won't change anything
	fragColor = colors[gl_VertexIndex];
}