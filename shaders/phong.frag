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

#define MAX_POINT_LIGHTS 8

struct PointLight {
	vec3 position;
	vec3 color;
	float constant;
	float linear;
	float quadratic;
};

uniform int numPointLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

uniform samplerCube shadowMap;
uniform float shadowFarPlane;

vec3 shadowSampleOffsets[20] = vec3[](
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float PointShadow(vec3 fragPos, vec3 lightPos)
{
    vec3 toFrag = fragPos - lightPos;
    float currentDepth = length(toFrag);
    float bias = currentDepth * 0.005 + 0.5;
    float shadow = 0.0;
    float diskRadius = currentDepth * 0.003;
    for (int i = 0; i < 20; ++i)
    {
        float closestDepth = texture(shadowMap, toFrag + shadowSampleOffsets[i] * diskRadius).r * shadowFarPlane;
        if (currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    return shadow / 20.0;
}

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

	vec3 ambientIntensity = material.x * ambientColor + 0.35;
	vec3 diffuseIntensity = vec3(0);
	vec3 specularIntensity = vec3(0);

	vec3 eyeDir = normalize(viewPos - FragWorldPos);
	for (int i = 0; i < numPointLights; i++)
	{
		vec3 toLight = pointLights[i].position - FragWorldPos;
		float dist = length(toLight);
		vec3 plDir = normalize(toLight);

		float attenuation = 1.0 / (pointLights[i].constant
			+ pointLights[i].linear * dist
			+ pointLights[i].quadratic * dist * dist);

		float lambert = dot(norm, plDir);
		if (lambert > 0)
		{
			float shadow = (i == 0) ? PointShadow(FragWorldPos, pointLights[i].position) : 0.0;
			float lit = (1.0 - shadow);

			diffuseIntensity += lit * material.y * pointLights[i].color * lambert * attenuation;

			vec3 reflectDir = normalize(reflect(-plDir, norm));
			float spec = dot(reflectDir, eyeDir);
			if (spec > 0)
			{
				specularIntensity += lit * material.z * pointLights[i].color * pow(spec, material.w) * attenuation;
			}
		}
	}

	vec3 lightIntensity = ambientIntensity + diffuseIntensity + specularIntensity;
	FragColor = vec4(lightIntensity, 1.0) * texture(baseTexture, TexCoord);
}
