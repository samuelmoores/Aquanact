#version 330 
layout (location=0) out vec4 FragColor;

in vec3 FragWorldPos;
in vec3 Normal;
in vec2 TexCoord;
in mat3 TBN;

uniform sampler2D baseTexture;
uniform sampler2D normalMap;

uniform vec4 material;
uniform vec3 ambientColor;
uniform vec3 directionalLight;
uniform vec3 directionalColor;
uniform vec3 viewPos;

void main()
{
	//vec3 sampledNormal = texture(normalMap, TexCoord).rgb;
	//sampledNormal = normalize(sampledNormal * 2.0 - 1.0);

	//vec3 norm = normalize(TBN * sampledNormal);

	vec3 norm = normalize(Normal);

	vec3 ambientIntensity = material.x * ambientColor;
	vec3 diffuseIntensity = vec3(0);
	vec3 specularIntensity = vec3(0);
	
	vec3 lightDir = -directionalLight;
	float lambertFactor = dot(norm, normalize(lightDir));

	if(lambertFactor > 0)
	{
		diffuseIntensity = material.y * directionalColor * lambertFactor;

		vec3 eyeDir = normalize(viewPos - FragWorldPos);
		vec3 reflectDir = normalize(reflect(-lightDir, norm));
		float spec = dot(reflectDir, eyeDir);

		if(spec > 0)
		{
			specularIntensity = material.z * directionalColor * pow(spec, material.w);
		}
	}

	vec3 lightIntensity = ambientIntensity + diffuseIntensity + specularIntensity;
	FragColor = vec4(lightIntensity, 1) * texture(baseTexture, TexCoord);
}