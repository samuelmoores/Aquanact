#version 330 core

layout (location = 0) in vec3 vPosition;
layout (location = 4) in ivec4 vBoneIDs;
layout (location = 5) in vec4 vWeights;

uniform mat4 model;
uniform mat4 shadowMatrix;
uniform bool skinned;
uniform mat4 finalBones[200];

out vec3 FragWorldPos;

void main()
{
    vec4 localPosition = vec4(vPosition, 1.0);
    if (skinned)
    {
        mat4 boneTransform = mat4(0.0);
        if (vBoneIDs[0] >= 0) boneTransform += finalBones[vBoneIDs[0]] * vWeights[0];
        if (vBoneIDs[1] >= 0) boneTransform += finalBones[vBoneIDs[1]] * vWeights[1];
        if (vBoneIDs[2] >= 0) boneTransform += finalBones[vBoneIDs[2]] * vWeights[2];
        if (vBoneIDs[3] >= 0) boneTransform += finalBones[vBoneIDs[3]] * vWeights[3];
        localPosition = boneTransform * localPosition;
    }

    vec4 worldPosition = model * localPosition;
    FragWorldPos = worldPosition.xyz;
    gl_Position = shadowMatrix * worldPosition;
}
