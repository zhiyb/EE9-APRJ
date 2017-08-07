#version 330

in vec3 vertex;
uniform vec2 move;
uniform mat4 projection;

uniform float zoom;
uniform int dimension;

in float intensity;
out vec3 colour;

void main(void)
{
	float size = 2.0 / float(dimension);
	vec3 pos = vertex * size * 0.75;
	vec2 offset = vec2(ivec2(gl_InstanceID % dimension, gl_InstanceID / dimension)) * size;
	pos.xy = pos.xy + offset - vec2(1.0, 1.0) - move;
	gl_Position = projection * vec4(pos * zoom, 1.0);
        colour = vec3(intensity / 255.0);
}
