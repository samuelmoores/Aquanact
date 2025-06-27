#version 330
layout (location=0) in vec3 vPosition;
layout (location=1) in vec3 vColor;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 VertexColor;

void main()
{
	gl_Position = projection * view * model * vec4(vPosition, 1.0);
	VertexColor = vColor;
}