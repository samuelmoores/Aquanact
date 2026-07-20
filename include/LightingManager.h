#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <ShaderProgram.h>

struct DirectionalLight {
	glm::vec3 direction = glm::vec3(-0.3f, -1.0f, 0.2f);
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float ambient = 0.5;
};

class LightingManager {
public:
	void startUp();
	void shutDown();
	void ApplyToShader(const ShaderProgram* shader) const;
	DirectionalLight& SunLight();
	const DirectionalLight& SunLight() const;

private:
	DirectionalLight m_sun;
};
