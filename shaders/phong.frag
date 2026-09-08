#version 330 core

struct DirectionalLight {
	vec3 direction;
	vec3 color;
	float intensity;
	float ambient;

};

struct PointLight {
	vec3 position;
	vec3 color;
	float intensity;
	float ambient;
	float radius;
	float radiusFade;

	float constant;
	float linear;
	float quadratic;
};

layout (location=0) out vec4 FragColor;

//vertex shader
in vec3 FragWorldPos;
in vec3 Normal;
in vec2 TexCoord;
in vec3 ViewPos;
in mat3 TBN;
in vec4 FragPosLightSpace;

//uniforms
uniform sampler2D baseTexture;
uniform sampler2D specularTexture;
uniform sampler2D normalTexture;
uniform bool hasBaseTexture;
uniform bool hasSpecularTexture;
uniform bool hasNormalTexture;
uniform DirectionalLight sunLight;
uniform int pointLightCount;
uniform PointLight pointLights[8];
uniform sampler2D shadowMap;
uniform bool directionalShadowEnabled;
uniform samplerCube pointShadowMap0;
uniform samplerCube pointShadowMap1;
uniform samplerCube pointShadowMap2;
uniform samplerCube pointShadowMap3;
uniform samplerCube pointShadowMap4;
uniform samplerCube pointShadowMap5;
uniform samplerCube pointShadowMap6;
uniform samplerCube pointShadowMap7;
uniform bool pointShadowReady[8];
uniform float pointShadowFarPlanes[8];

const vec3 PointShadowSampleOffsets[20] = vec3[](
	vec3(1.0, 1.0, 1.0), vec3(1.0, 1.0, -1.0),
	vec3(1.0, -1.0, 1.0), vec3(1.0, -1.0, -1.0),
	vec3(-1.0, 1.0, 1.0), vec3(-1.0, 1.0, -1.0),
	vec3(-1.0, -1.0, 1.0), vec3(-1.0, -1.0, -1.0),
	vec3(1.0, 1.0, 0.0), vec3(1.0, -1.0, 0.0),
	vec3(-1.0, 1.0, 0.0), vec3(-1.0, -1.0, 0.0),
	vec3(1.0, 0.0, 1.0), vec3(1.0, 0.0, -1.0),
	vec3(-1.0, 0.0, 1.0), vec3(-1.0, 0.0, -1.0),
	vec3(0.0, 1.0, 1.0), vec3(0.0, 1.0, -1.0),
	vec3(0.0, -1.0, 1.0), vec3(0.0, -1.0, -1.0)
);

float CalculateShadow(vec3 geometricNormal, vec3 lightDirection)
{
	if (!directionalShadowEnabled)
	{
		return 0.0;
	}

	vec3 projected = FragPosLightSpace.xyz / FragPosLightSpace.w;
	projected = projected * 0.5 + 0.5;
	if (projected.z <= 0.0 || projected.z >= 1.0 || projected.x <= 0.0 || projected.x >= 1.0 || projected.y <= 0.0 || projected.y >= 1.0)
	{
		return 0.0;
	}

	float normalAlignment = clamp(dot(geometricNormal, lightDirection), 0.0, 1.0);
	float bias = max(0.0025 * (1.0 - normalAlignment), 0.00035);
	vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
	float shadow = 0.0;
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			float closestDepth = texture(shadowMap, projected.xy + vec2(x, y) * texelSize).r;
			shadow += projected.z - bias > closestDepth ? 1.0 : 0.0;
		}
	}
	return shadow / 9.0;
}

float SamplePointShadowMap(int lightIndex, vec3 direction)
{
	if (lightIndex == 0) return texture(pointShadowMap0, direction).r;
	if (lightIndex == 1) return texture(pointShadowMap1, direction).r;
	if (lightIndex == 2) return texture(pointShadowMap2, direction).r;
	if (lightIndex == 3) return texture(pointShadowMap3, direction).r;
	if (lightIndex == 4) return texture(pointShadowMap4, direction).r;
	if (lightIndex == 5) return texture(pointShadowMap5, direction).r;
	if (lightIndex == 6) return texture(pointShadowMap6, direction).r;
	return texture(pointShadowMap7, direction).r;
}

float InterleavedGradientNoise(vec2 pixelPosition)
{
	// Half an 8-bit color step is enough to break up coherent radial bands while
	// remaining visually imperceptible as noise at normal viewing distances.
	return fract(52.9829189 * fract(dot(pixelPosition, vec2(0.06711056, 0.00583715))));
}

float CalculatePointShadow(int lightIndex, PointLight light, vec3 geometricNormal, vec3 lightDirection)
{
	if (!pointShadowReady[lightIndex])
	{
		return 0.0;
	}

	vec3 fragToLight = FragWorldPos - light.position;
	float currentDepth = length(fragToLight);
	float farPlane = pointShadowFarPlanes[lightIndex];
	if (farPlane <= 0.0 || currentDepth >= farPlane)
	{
		return 0.0;
	}

	float normalAlignment = clamp(dot(geometricNormal, lightDirection), 0.0, 1.0);
	// Compare in world units so changing a point light's radius does not also
	// change its effective shadow bias. Smooth each comparison to avoid the
	// discrete binary-sample intensity steps that produced visible contours.
	float bias = 0.05 + 0.35 * (1.0 - normalAlignment);
	float filterWidth = max(currentDepth * 0.0015, 0.04);
	float sampleRadius = max(currentDepth * 0.003, 0.002);
	float shadow = 0.0;
	for (int sampleIndex = 0; sampleIndex < 20; ++sampleIndex)
	{
		float closestDepth = SamplePointShadowMap(
			lightIndex,
			fragToLight + PointShadowSampleOffsets[sampleIndex] * sampleRadius) * farPlane;
		float depthDifference = currentDepth - bias - closestDepth;
		// Keep equal/self depth fully lit. A symmetric transition around zero
		// partially shadowed the receiver itself, and its distance-scaled width
		// showed up as faint concentric rings around the point light.
		shadow += smoothstep(0.0, filterWidth, depthDifference);
	}
	return shadow / 20.0;
}

vec3 CalculateDirectionalLight(vec3 baseColor, vec3 specularStrength, vec3 vertexNormal, vec3 geometricNormal, vec3 viewDirection)
{
	vec3 lightDir = normalize(-sunLight.direction);
	float shadow = CalculateShadow(geometricNormal, lightDir);
	float diffuseStrength = max(dot(vertexNormal, lightDir), 0.0);
	vec3 reflectDirection = reflect(-lightDir, vertexNormal);
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32.0);

	vec3 ambient = baseColor * sunLight.color * sunLight.ambient;
	vec3 diffuse = baseColor * sunLight.color * diffuseStrength;
	vec3 specular = specularStrength * sunLight.color * spec;
	return (ambient + (1.0 - shadow) * (diffuse + specular)) * sunLight.intensity;
}

vec3 CalculatePointLight(int lightIndex, PointLight light, vec3 baseColor, vec3 specularStrength, vec3 vertexNormal, vec3 geometricNormal, vec3 viewDirection)
{
	vec3 lightOffset = light.position - FragWorldPos;
	float lightDistance = length(lightOffset);
	float radiusFade = 1.0;
	if (light.radius > 0.0)
	{
		float fadeStart = light.radius * clamp(light.radiusFade, 0.0, 1.0);
		radiusFade = 1.0 - smoothstep(fadeStart, light.radius, lightDistance);
	}

	vec3 lightDir = normalize(lightOffset);
	float shadow = CalculatePointShadow(lightIndex, light, geometricNormal, lightDir);
	float attenuation = 1.0 / (light.constant + light.linear * lightDistance + light.quadratic * lightDistance * lightDistance);
	float diffuseStrength = max(dot(vertexNormal, lightDir), 0.0);
	vec3 reflectDirection = reflect(-lightDir, vertexNormal);
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32.0);

	vec3 ambient = baseColor * light.color * light.ambient;
	vec3 diffuse = baseColor * light.color * diffuseStrength;
	vec3 specular = specularStrength * light.color * spec;
	return (ambient + (1.0 - shadow) * (diffuse + specular)) * attenuation * radiusFade * light.intensity;
}

void main()
{
	vec3 geometricNormal = normalize(Normal);
	vec3 vertexNormal = geometricNormal;
	if (hasNormalTexture)
	{
		vec3 tangentNormal = texture(normalTexture, TexCoord).rgb * 2.0 - 1.0;
		vertexNormal = normalize(TBN * tangentNormal);
	}

	vec3 viewDirection = normalize(ViewPos - FragWorldPos);
	vec3 baseColor = hasBaseTexture ? texture(baseTexture, TexCoord).rgb : vec3(0.20);
	vec3 specularStrength = hasSpecularTexture ? texture(specularTexture, TexCoord).rgb : vec3(0.0);

	vec3 litColor = CalculateDirectionalLight(baseColor, specularStrength, vertexNormal, geometricNormal, viewDirection);
	
	for (int i = 0; i < pointLightCount; ++i)
	{
		litColor += CalculatePointLight(i, pointLights[i], baseColor, specularStrength, vertexNormal, geometricNormal, viewDirection);
	}
	
	const float outputColorStep = 1.0 / 255.0;
	float dither = (InterleavedGradientNoise(gl_FragCoord.xy) - 0.5) * outputColorStep;
	FragColor = vec4(max(litColor + vec3(dither), vec3(0.0)), 1.0);
}
