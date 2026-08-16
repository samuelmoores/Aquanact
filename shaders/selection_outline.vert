#version 330 core

layout (location = 0) in vec3 vPosition;
layout (location = 2) in vec3 vNormal;
layout (location = 4) in ivec4 vBoneIDs;
layout (location = 5) in vec4 vWeights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform bool skinned;
uniform mat4 finalBones[200];
uniform vec2 viewportSize;
uniform float outlineWidth;

void main()
{
	mat4 boneTransform = mat4(0.0);
	if (vBoneIDs[0] >= 0) boneTransform += finalBones[vBoneIDs[0]] * vWeights[0];
	if (vBoneIDs[1] >= 0) boneTransform += finalBones[vBoneIDs[1]] * vWeights[1];
	if (vBoneIDs[2] >= 0) boneTransform += finalBones[vBoneIDs[2]] * vWeights[2];
	if (vBoneIDs[3] >= 0) boneTransform += finalBones[vBoneIDs[3]] * vWeights[3];

	vec4 localPosition = vec4(vPosition, 1.0);
	vec3 localNormal = vNormal;
	if (skinned)
	{
		localPosition = boneTransform * localPosition;
		localNormal = mat3(transpose(inverse(boneTransform))) * localNormal;
	}

	mat4 viewModel = view * model;
	vec4 clipPosition = projection * viewModel * localPosition;
	vec3 viewNormal = normalize(mat3(transpose(inverse(viewModel))) * localNormal);
	vec2 projectedNormal = (projection * vec4(viewNormal, 0.0)).xy;
	float normalLength = length(projectedNormal);
	if (normalLength > 1e-5)
		projectedNormal /= normalLength;
	else
		projectedNormal = vec2(0.0);

	vec2 safeViewport = max(viewportSize, vec2(1.0));
	clipPosition.xy += projectedNormal * (2.0 * outlineWidth / safeViewport) * clipPosition.w;
	gl_Position = clipPosition;
}
