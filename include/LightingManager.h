#pragma once

#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <ShaderProgram.h>

struct DirectionalLight {
	glm::vec3 direction = glm::vec3(-0.3f, -1.0f, 0.2f);
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float ambient = 0.5;
};

struct PointLight {
	glm::vec3 position = glm::vec3(0.0f, 2.0f, 0.0f);
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float ambient = 0.0f;
	float radius = 500.0f;
	float radiusFade = 0.75f;

	float constant = 1.0f;
	float linear = 0.004f;
	float quadratic = 0.000004f;

	void SetRadius(float newRadius);
};

class LightingManager {
public:
	static constexpr int MaxPointLights = 8;

	void startUp();
	void shutDown();
	void ApplyToShader(const ShaderProgram* shader) const;
	DirectionalLight& SunLight();
	const DirectionalLight& SunLight() const;
	PointLight& AddPointLight();
	std::vector<PointLight>& PointLights();
	const std::vector<PointLight>& PointLights() const;

private:
	DirectionalLight m_sun;
	std::vector<PointLight> m_pointLights;
};
