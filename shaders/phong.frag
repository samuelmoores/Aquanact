#version 330 core
layout (location=0) out vec4 FragColor;

in vec3 FragWorldPos;
in vec3 Normal;
in vec2 TexCoord;
in mat3 TBN;
flat in ivec4 BoneIDs;
in vec4 Weights;

uniform sampler2D baseTexture;
uniform sampler2D normalMap;
uniform bool useNormalMap;

uniform vec4 material;
uniform vec3 ambientColor;
uniform vec3 viewPos;
uniform int bone;


void main()
{
	vec3 norm;

	if (useNormalMap)
	{
	    vec3 tangentNormal = texture(normalMap, TexCoord).rgb * 2.0 - 1.0;
	    norm = normalize(TBN * tangentNormal);
	}
	else
	{
	    norm = normalize(Normal);
	}

	vec3 ambientIntensity = material.x * ambientColor + 0.02;
	FragColor = vec4(ambientIntensity, 1.0) * texture(baseTexture, TexCoord);
}
