#version 330 core

in vec3 FragWorldPos;

uniform vec3 lightPosition;
uniform float farPlane;

void main()
{
    gl_FragDepth = length(FragWorldPos - lightPosition) / farPlane;
}
