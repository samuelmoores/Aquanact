#version 330 core

layout (location = 0) in vec3 vPosition;

uniform mat4 viewProjection;
uniform mat4 model;

void main()
{
    gl_Position = viewProjection * model * vec4(vPosition, 1.0);
}
