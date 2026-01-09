#version 330 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vTexCoord;
layout (location = 2) in vec3 vNormal;
layout (location = 3) in ivec4 vBoneIDs;
layout (location = 4) in vec4 vWeights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform bool skinned;
uniform mat4 finalBones[200];

out vec2 TexCoord;
out vec3 FragWorldPos;
out vec3 Normal;
flat out ivec4 BoneIDs;
out vec4 Weights;

out mat3 TBN;

void main()
{
    BoneIDs = vBoneIDs;
    Weights = vWeights;

    mat4 BoneTransform = mat4(0.0);

    BoneTransform = finalBones[BoneIDs[0]] * Weights[0];
    BoneTransform     += finalBones[BoneIDs[1]] * Weights[1];
    BoneTransform     += finalBones[BoneIDs[2]] * Weights[2];
    BoneTransform     += finalBones[BoneIDs[3]] * Weights[3];
 
    vec4 PosL = vec4(vPosition, 1.0);

    if(skinned)
        PosL = BoneTransform * vec4(vPosition, 1.0);
    

    // Final vertex position
    gl_Position = projection * view * model * PosL;

    // World-space position
    FragWorldPos = vec3(model);

    // Pass texture coordinates
    TexCoord = vTexCoord;

    Normal = mat3(transpose(inverse(model))) * vNormal;

    mat3 normalMatrix = transpose(inverse(mat3(model)));

    //vec3 N = normalize(normalMatrix * vNormal);
    //vec3 T = normalize(normalMatrix * vTangent);
    //vec3 B = normalize(cross(N, T)); // safest way

    //TBN = mat3(T, B, N);
}
