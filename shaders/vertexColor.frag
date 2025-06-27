#version 330
in vec3 VertexColor;

layout (location=0) out vec4 FragColor;

void main()
{
	FragColor = vec4(VertexColor, 1.0);
}