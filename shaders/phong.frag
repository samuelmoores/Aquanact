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

//uniforms
uniform sampler2D baseTexture;
uniform DirectionalLight sunLight;

void main()
{
	//ambient

	//diffuse
	vec3 vertexNormal = normalize(Normal);
	vec3 lightDir = normalize(-sunLight.direction);
	float dotProduct = max(dot(vertexNormal, lightDir), sunLight.ambient);

	//specular
	float specularStrength = 0.5f;
	vec3 viewDirection = normalize(ViewPos - FragWorldPos);
	vec3 reflectDirection = reflect(-lightDir, vertexNormal);
	float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32);
	vec3 specular = specularStrength * spec * sunLight.color;

	vec3 finalTextureFrag = texture(baseTexture, TexCoord).rgb * dotProduct;
	
	FragColor = vec4(finalTextureFrag + specular, 1.0) * sunLight.intensity;
	return;
}
