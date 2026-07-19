#version 330 core
layout (location=0) out vec4 FragColor;

struct DirectionalLight {
	vec3 direction;
	vec3 color;
	float intensity;
};

in vec3 FragWorldPos;
in vec3 Normal;
in vec2 TexCoord;
in mat3 TBN;
flat in ivec4 BoneIDs;
in vec4 Weights;

uniform sampler2D baseTexture;
uniform sampler2D normalMap;
uniform bool useNormalMap;

uniform DirectionalLight sunLight;
uniform vec4 material;
uniform vec3 ambientColor;
uniform vec3 viewPos;
uniform int bone;


void main()
{
	vec3 norm;

	if (useNormalMap && false)
	{
	    vec3 tangentNormal = texture(normalMap, TexCoord).rgb * 2.0 - 1.0;
	    norm = normalize(TBN * tangentNormal);
	}
	else
	{
	    norm = normalize(Normal);
	}

	vec3 lightDir = normalize(-sunLight.direction);

	float diff = max(dot(norm, lightDir), 0.0);

	vec3 result = (ambientColor) * texture(baseTexture, TexCoord).rgb * diff;

	vec3 finalColor = ambientColor + sunLight.color;

	vec3 ambientIntensity = material.x * finalColor + 0.02;
	//vec3 ambientIntensity = finalColor + 0.02;
	vec3 finalIntensity = ambientIntensity + sunLight.intensity;

	FragColor = vec4(finalIntensity, 1.0) * vec4(result, 1.0);
	//FragColor = vec4(texture(baseTexture, TexCoord).rgb, 1.0);
}
