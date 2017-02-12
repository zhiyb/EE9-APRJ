#version 130

in vec3 colour;
out vec4 fragColour;

void main(void)
{
	//fragColour = vec4(0.2, 0.4, 1.0, 1.0);
	fragColour = vec4(colour, 1.0);
}
