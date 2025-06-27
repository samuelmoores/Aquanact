#version 330 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vTexCoord;
layout (location = 2) in vec3 vNormal;
layout (location = 3) in vec3 vTangent;
layout (location = 4) in vec3 vBitangent;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec2 TexCoord;
out vec3 FragWorldPos;
out vec3 Normal;

out mat3 TBN;

void main()
{
    // Final vertex position
    gl_Position = projection * view * model * vec4(vPosition, 1.0);

    // World-space position
    FragWorldPos = vec3(model * vec4(vPosition, 1.0));

    // Pass texture coordinates
    TexCoord = vTexCoord;

    Normal = mat3(transpose(inverse(model))) * vNormal;

    mat3 normalMatrix = transpose(inverse(mat3(model)));

    vec3 N = normalize(normalMatrix * vNormal);
    vec3 T = normalize(normalMatrix * vTangent);
    vec3 B = normalize(cross(N, T)); // safest way

    TBN = mat3(T, B, N);
}
