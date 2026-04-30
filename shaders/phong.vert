#version 330 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec2 vTexCoord;
layout (location = 2) in vec3 vNormal;
layout (location = 3) in vec3 vTangent;
layout (location = 4) in ivec4 vBoneIDs;
layout (location = 5) in vec4 vWeights;

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

    if (BoneIDs[0] >= 0) BoneTransform += finalBones[BoneIDs[0]] * Weights[0];
    if (BoneIDs[1] >= 0) BoneTransform += finalBones[BoneIDs[1]] * Weights[1];
    if (BoneIDs[2] >= 0) BoneTransform += finalBones[BoneIDs[2]] * Weights[2];
    if (BoneIDs[3] >= 0) BoneTransform += finalBones[BoneIDs[3]] * Weights[3];
 
    vec4 PosL = vec4(vPosition, 1.0);

    if(skinned)
        PosL = BoneTransform * vec4(vPosition, 1.0);
    

    // Final vertex position
    gl_Position = projection * view * model * PosL;

    // World-space position
    FragWorldPos = vec3(model * PosL);

    // Transform normal to world space
    // For skinned meshes, apply bone transform first so normals follow the skeleton
    vec3 localNormal  = vNormal;
    vec3 localTangent = vTangent;
    if (skinned)
    {
        mat3 boneNormalMatrix = mat3(transpose(inverse(BoneTransform)));
        localNormal  = boneNormalMatrix * vNormal;
        localTangent = boneNormalMatrix * vTangent;
    }

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 N = normalize(normalMatrix * localNormal);
    vec3 T = normalize(normalMatrix * localTangent);
    T = normalize(T - dot(T, N) * N); // re-orthogonalize
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
    Normal = N;

    // Pass texture coordinates
    TexCoord = vTexCoord;

}
