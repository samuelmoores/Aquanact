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

//uniforms
uniform sampler2D baseTexture;
uniform sampler2D specularTexture;
uniform sampler2D normalTexture;
uniform bool hasNormalTexture;
uniform DirectionalLight sunLight;
uniform int pointLightCount;
uniform PointLight pointLights[8];

vec3 CalculateDirectionalLight(vec3 baseColor, vec3 specularStrength, vec3 vertexNormal, vec3 viewDirection)
{
	vec3 lightDir = normalize(-sunLight.direction);
	float diffuseStrength = max(dot(vertexNormal, lightDir), 0.0);
	vec3 reflectDirection = reflect(-lightDir, vertexNormal);
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32.0);

	vec3 ambient = baseColor * sunLight.color * sunLight.ambient;
	vec3 diffuse = baseColor * sunLight.color * diffuseStrength;
	vec3 specular = specularStrength * sunLight.color * spec;
	return (ambient + diffuse + specular) * sunLight.intensity;
}

vec3 CalculatePointLight(PointLight light, vec3 baseColor, vec3 specularStrength, vec3 vertexNormal, vec3 viewDirection)
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
	float attenuation = 1.0 / (light.constant + light.linear * lightDistance + light.quadratic * lightDistance * lightDistance);
	float diffuseStrength = max(dot(vertexNormal, lightDir), 0.0);
	vec3 reflectDirection = reflect(-lightDir, vertexNormal);
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32.0);

	vec3 ambient = baseColor * light.color * light.ambient;
	vec3 diffuse = baseColor * light.color * diffuseStrength;
	vec3 specular = specularStrength * light.color * spec;
	return (ambient + diffuse + specular) * attenuation * radiusFade * light.intensity;
}

void main()
{
	vec3 vertexNormal = normalize(Normal);
	if (hasNormalTexture)
	{
		vec3 tangentNormal = texture(normalTexture, TexCoord).rgb * 2.0 - 1.0;
		vertexNormal = normalize(TBN * tangentNormal);
	}

	vec3 viewDirection = normalize(ViewPos - FragWorldPos);
	vec3 baseColor = texture(baseTexture, TexCoord).rgb;
	vec3 specularStrength = texture(specularTexture, TexCoord).rgb;

	vec3 litColor = CalculateDirectionalLight(baseColor, specularStrength, vertexNormal, viewDirection);
	
	for (int i = 0; i < pointLightCount; ++i)
	{
		litColor += CalculatePointLight(pointLights[i], baseColor, specularStrength, vertexNormal, viewDirection);
	}
	
	FragColor = vec4(litColor, 1.0);
}
