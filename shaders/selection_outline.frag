#version 330 core

uniform vec4 outlineColor;

layout (location = 0) out vec4 FragColor;

void main()
{
	FragColor = outlineColor;
}
