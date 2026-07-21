#version 330 core

struct DirectionalLight {
	vec3 direction;
	vec3 color;
	float intensity;
	float ambient;

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
uniform DirectionalLight sunLight;

void main()
{
	//ambient from sunLight

	//diffuse
	vec3 tangentNormal = texture(normalTexture, TexCoord).rgb * 2.0 - 1.0;
	vec3 vertexNormal = normalize(TBN * tangentNormal);
	vec3 lightDir = normalize(-sunLight.direction);
	float dotProduct = max(dot(vertexNormal, lightDir), sunLight.ambient);

	//specular
	vec3 specularStrength = texture(specularTexture, TexCoord).rgb;
	vec3 viewDirection = normalize(ViewPos - FragWorldPos);
	vec3 reflectDirection = reflect(-lightDir, vertexNormal);
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32);
	vec3 specular = specularStrength * spec * sunLight.color;

	vec3 finalTextureFrag = texture(baseTexture, TexCoord).rgb * dotProduct;
	
	FragColor = vec4(finalTextureFrag + specular, 1.0) * sunLight.intensity;
	return;
}
