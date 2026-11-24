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
struct SpotLight {
	vec3 position;
	vec3 direction;
	vec3 color;
	float intensity;
	float ambient;
	float radius;
	float radiusFade;
	float innerCutoff;
	float outerCutoff;
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
uniform sampler2D roughnessTexture;
uniform sampler2D heightTexture;
uniform vec4 material;
uniform bool emissiveEnabled;
uniform vec3 emissiveColor;
uniform float emissiveBlendFactor;
uniform bool hasBaseTexture;
uniform bool hasSpecularTexture;
uniform bool hasNormalTexture;
uniform bool hasRoughnessTexture;
uniform bool hasHeightTexture;
uniform DirectionalLight sunLight;
uniform int pointLightCount;
uniform PointLight pointLights[8];
uniform int spotLightCount;
uniform SpotLight spotLights[8];
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
uniform sampler2DArray spotShadowMap;
uniform bool spotShadowReady[8];
uniform mat4 spotLightSpaceMatrices[8];

vec2 PointShadowSampleOffset(int sampleIndex)
{
	// A golden-angle disk distributes the filter taps without a repeating
	// polygonal pattern on large, flat receivers.
	float index = float(sampleIndex);
	float radialDistance = sqrt((index + 0.5) / 20.0);
	float angle = 2.39996323 * index;
	return vec2(cos(angle), sin(angle)) * radialDistance;
}

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
	float bias = max(0.004 * (1.0 - normalAlignment), 0.0008);
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
	float bias = 0.15 + 0.5 * (1.0 - normalAlignment);
	float filterWidth = max(light.radius * 0.00075, 0.04);
	// Spread the cubemap taps across a broader region to soften point-light
	// shadow boundaries at the current 512x512 shadow-map resolution. Using
	// the light's range as a stable world-space proxy avoids shadows getting
	// noticeably harder when a receiver moves close to the light.
	float sampleRadius = max(light.radius * 0.01, 0.002);
	vec3 sampleAxis = normalize(fragToLight);
	vec3 referenceAxis = abs(sampleAxis.z) < 0.999
		? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
	vec3 tangent = normalize(cross(sampleAxis, referenceAxis));
	vec3 bitangent = cross(sampleAxis, tangent);
	float rotation = InterleavedGradientNoise(gl_FragCoord.xy + vec2(float(lightIndex) * 17.0, 31.0)) * 6.2831853;
	float rotationCos = cos(rotation);
	float rotationSin = sin(rotation);
	float shadow = 0.0;
	for (int sampleIndex = 0; sampleIndex < 20; ++sampleIndex)
	{
		vec2 localOffset = PointShadowSampleOffset(sampleIndex);
		vec2 rotatedTangentOffset = vec2(
			localOffset.x * rotationCos - localOffset.y * rotationSin,
			localOffset.x * rotationSin + localOffset.y * rotationCos);
		vec3 worldOffset = tangent * rotatedTangentOffset.x + bitangent * rotatedTangentOffset.y;
		float closestDepth = SamplePointShadowMap(
			lightIndex,
			fragToLight + worldOffset * sampleRadius) * farPlane;
		float depthDifference = currentDepth - bias - closestDepth;
		// Keep equal/self depth fully lit. A symmetric transition around zero
		// can partially shadow the receiver itself and cause a visible halo.
		shadow += smoothstep(0.0, filterWidth, depthDifference);
	}
	return shadow / 20.0;
}

vec3 CalculateDirectionalLight(vec3 baseColor, vec3 specularStrength, float shininess, vec3 vertexNormal, vec3 geometricNormal, vec3 viewDirection)
{
	vec3 lightDir = normalize(-sunLight.direction);
	float shadow = CalculateShadow(geometricNormal, lightDir);
	float diffuseStrength = max(dot(vertexNormal, lightDir), 0.0);
	vec3 reflectDirection = reflect(-lightDir, vertexNormal);
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), shininess);

	vec3 ambient = baseColor * sunLight.color * sunLight.ambient;
	vec3 diffuse = baseColor * sunLight.color * diffuseStrength;
	vec3 specular = specularStrength * sunLight.color * spec;
	return (ambient + (1.0 - shadow) * (diffuse + specular)) * sunLight.intensity;
}

vec3 CalculatePointLight(int lightIndex, PointLight light, vec3 baseColor, vec3 specularStrength, float shininess, vec3 vertexNormal, vec3 geometricNormal, vec3 viewDirection)
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
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), shininess);

	vec3 ambient = baseColor * light.color * light.ambient;
	vec3 diffuse = baseColor * light.color * diffuseStrength;
	vec3 specular = specularStrength * light.color * spec;
	return (ambient + (1.0 - shadow) * (diffuse + specular)) * attenuation * radiusFade * light.intensity;
}

vec3 CalculateSpotLight(SpotLight light, vec3 baseColor, vec3 specularStrength,
	float shininess, vec3 vertexNormal, vec3 geometricNormal, vec3 viewDirection, int lightIndex)
{
	vec3 lightOffset = light.position - FragWorldPos;
	float lightDistance = length(lightOffset);
	vec3 lightDir = normalize(lightOffset);
	float coneCosine = dot(normalize(FragWorldPos - light.position), normalize(light.direction));
	float coneFactor = smoothstep(light.outerCutoff, light.innerCutoff, coneCosine);
	float radiusFadeStart = light.radius * clamp(light.radiusFade, 0.0, 1.0);
	float radiusFactor = 1.0 - smoothstep(radiusFadeStart, light.radius, lightDistance);
	float attenuation = 1.0 / max(light.constant + light.linear * lightDistance +
		light.quadratic * lightDistance * lightDistance, 0.001);
	float diffuseStrength = max(dot(vertexNormal, lightDir), 0.0);
	vec3 reflectDirection = reflect(-lightDir, vertexNormal);
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), shininess);
	vec3 ambient = baseColor * light.color * light.ambient;
	vec3 diffuse = baseColor * light.color * diffuseStrength;
	vec3 specular = specularStrength * light.color * spec;
	float shadow = 0.0;
	if (spotShadowReady[lightIndex])
	{
		vec4 lightSpacePosition = spotLightSpaceMatrices[lightIndex] * vec4(FragWorldPos, 1.0);
		vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
		projected = projected * 0.5 + 0.5;
		if (projected.z > 0.0 && projected.z < 1.0 && lightDistance < light.radius &&
			projected.x >= 0.0 && projected.x <= 1.0 &&
			projected.y >= 0.0 && projected.y <= 1.0)
		{
			float normalAlignment = clamp(dot(geometricNormal, lightDir), 0.0, 1.0);
			float bias = 0.15 + 0.5 * (1.0 - normalAlignment);
			float filterWidth = max(light.radius * 0.00075, 0.04);
			vec2 texelSize = 1.0 / vec2(textureSize(spotShadowMap, 0).xy);
			float coneTangent = max(tan(acos(clamp(light.outerCutoff, -1.0, 1.0))), 0.001);
			float sampleRadius = clamp(light.radius * 0.01 /
				(2.0 * max(lightDistance, 0.1) * coneTangent), texelSize.x * 1.5, 0.08);
			float rotation = InterleavedGradientNoise(gl_FragCoord.xy +
				vec2(float(lightIndex) * 17.0, 31.0)) * 6.2831853;
			float rotationCos = cos(rotation);
			float rotationSin = sin(rotation);
			for (int sampleIndex = 0; sampleIndex < 20; ++sampleIndex)
			{
				vec2 localOffset = PointShadowSampleOffset(sampleIndex);
				vec2 rotatedOffset = vec2(
					localOffset.x * rotationCos - localOffset.y * rotationSin,
					localOffset.x * rotationSin + localOffset.y * rotationCos);
				float closestDepth = texture(spotShadowMap,
					vec3(projected.xy + rotatedOffset * sampleRadius, float(lightIndex))).r * max(light.radius, 1.0);
				shadow += smoothstep(0.0, filterWidth, lightDistance - bias - closestDepth);
			}
			shadow /= 20.0;
		}
	}
	return (ambient + (1.0 - shadow) * (diffuse + specular)) * attenuation * radiusFactor * coneFactor * light.intensity;
}

void main()
{
	vec2 materialTexCoord = TexCoord;
	if (hasHeightTexture)
	{
		vec3 viewDirectionTangent = normalize(transpose(TBN) * normalize(ViewPos - FragWorldPos));
		float height = texture(heightTexture, TexCoord).r;
		float parallaxScale = 0.04;
		materialTexCoord -= (viewDirectionTangent.xy / max(abs(viewDirectionTangent.z), 0.15)) *
			((height - 0.5) * parallaxScale);
	}
	vec3 geometricNormal = normalize(Normal);
	vec3 vertexNormal = geometricNormal;
	if (hasNormalTexture)
	{
		vec3 tangentNormal = texture(normalTexture, materialTexCoord).rgb * 2.0 - 1.0;
		vertexNormal = normalize(TBN * tangentNormal);
	}

	vec3 viewDirection = normalize(ViewPos - FragWorldPos);
	vec3 baseColor = hasBaseTexture ? texture(baseTexture, materialTexCoord).rgb : vec3(0.20);
	// Keep the material's ordinary specular response when no specular mask is
	// assigned. A specular map then modulates that response per fragment, just
	// like the diffuse/specular map setup in LearnOpenGL's lighting chapter.
	vec3 specularStrength = vec3(material.z);
	if (hasSpecularTexture)
		specularStrength *= texture(specularTexture, materialTexCoord).rgb;
	// FBX roughness maps are commonly scalar maps stored in the red channel.
	// When the map is disabled, recover roughness from the imported Phong
	// shininess value so the fallback slider controls specular energy as well.
	float fallbackRoughness = sqrt(clamp(
		(128.0 - clamp(material.w, 2.0, 128.0)) / 126.0, 0.0, 1.0));
	float roughness = hasRoughnessTexture
		? clamp(texture(roughnessTexture, materialTexCoord).r, 0.0, 1.0)
		: fallbackRoughness;
	// A roughness map is the inverse of a shininess map: black gives a tight
	// specular highlight and white gives a broad one. Keep imported shininess as
	// the fallback when no roughness map is connected.
	float shininess = hasRoughnessTexture
		? mix(128.0, 2.0, roughness * roughness)
		: max(material.w, 1.0);
	// Phong shininess alone still creates a broad highlight at full roughness.
	// Fade its strength too, reaching zero for a completely rough surface.
	specularStrength *= (1.0 - roughness) * (1.0 - roughness);

	vec3 litColor = CalculateDirectionalLight(baseColor, specularStrength, shininess, vertexNormal, geometricNormal, viewDirection);
	
	for (int i = 0; i < pointLightCount; ++i)
	{
		litColor += CalculatePointLight(i, pointLights[i], baseColor, specularStrength, shininess, vertexNormal, geometricNormal, viewDirection);
	}
	for (int i = 0; i < spotLightCount; ++i)
	{
		litColor += CalculateSpotLight(spotLights[i], baseColor, specularStrength, shininess,
			vertexNormal, geometricNormal, viewDirection, i);
	}
	if (emissiveEnabled)
		litColor = mix(litColor, emissiveColor, clamp(emissiveBlendFactor, 0.0, 1.0));
	
	const float outputColorStep = 1.0 / 255.0;
	float dither = (InterleavedGradientNoise(gl_FragCoord.xy) - 0.5) * outputColorStep;
	FragColor = vec4(max(litColor + vec3(dither), vec3(0.0)), 1.0);
}
