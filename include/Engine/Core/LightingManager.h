#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Engine/Core/ShaderProgram.h"

struct DirectionalLight {
	std::string name = "Directional Light";
	glm::vec3 direction = glm::vec3(-0.3f, -1.0f, 0.2f);
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float ambient = 0.5;
	bool castsShadows = true;
};

struct PointLight {
	std::string name = "Point Light";
	glm::vec3 position = glm::vec3(0.0f, 2.0f, 0.0f);
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float ambient = 0.0f;
	float radius = 500.0f;
	float radiusFade = 0.75f;

	float constant = 1.0f;
	float linear = 0.004f;
	float quadratic = 0.000004f;
	bool castsShadows = false;

	void SetRadius(float newRadius);
};

struct SpotLight {
	std::string name = "Spot Light";
	glm::vec3 position{ 0.0f, 2.0f, 0.0f };
	glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
	glm::vec3 color{ 1.0f };
	float intensity = 1.0f;
	float ambient = 0.0f;
	float radius = 500.0f;
	float radiusFade = 0.75f;
	float innerAngle = 12.5f;
	float outerAngle = 17.5f;
	float constant = 1.0f;
	float linear = 0.004f;
	float quadratic = 0.000004f;
	bool castsShadows = false;

	void SetRadius(float newRadius);
	void SetAngles(float newInnerAngle, float newOuterAngle);
};

class LightingManager {
public:
	static constexpr int MaxPointLights = 8;
	static constexpr int MaxSpotLights = 8;

	void startUp();
	void shutDown();
	void ResetToDefaults();
	void ApplyToShader(const ShaderProgram* shader) const;
	DirectionalLight& SunLight();
	const DirectionalLight& SunLight() const;
	bool ShadowsEnabled() const;
	void SetShadowsEnabled(bool enabled);
	PointLight& AddPointLight();
	std::vector<PointLight>& PointLights();
	const std::vector<PointLight>& PointLights() const;
	SpotLight& AddSpotLight();
	std::vector<SpotLight>& SpotLights();
	const std::vector<SpotLight>& SpotLights() const;

private:
	DirectionalLight m_sun;
	bool m_shadowsEnabled = true;
	std::vector<PointLight> m_pointLights;
	std::vector<SpotLight> m_spotLights;
};


