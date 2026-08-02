#include "Engine/Core/LightingManager.h"

#include <algorithm>
#include <glm/common.hpp>
#include <string>

void PointLight::SetRadius(float newRadius)
{
	radius = glm::max(newRadius, 0.001f);
	constant = 1.0f;
	linear = 2.0f / radius;
	quadratic = 1.0f / (radius * radius);
}

void LightingManager::startUp()
{
	std::cout << "LightingManager startUp:\n";
	std::cout << "direction: " << m_sun.direction.x << ", " << m_sun.direction.y << ", " << m_sun.direction.z << std::endl;
	std::cout << "color: " << m_sun.color.x << ", " << m_sun.color.y << ", " << m_sun.color.z << std::endl;
	std::cout << "intensity: " << m_sun.intensity << std::endl;
}

void LightingManager::shutDown()
{
}

void LightingManager::ApplyToShader(const ShaderProgram* shader) const
{
	shader->setUniform("sunLight.direction", m_sun.direction);
	shader->setUniform("sunLight.color", m_sun.color);
	shader->setUniform("sunLight.intensity", m_sun.intensity);
	shader->setUniform("sunLight.ambient", m_sun.ambient);

	const int pointLightCount = std::min(static_cast<int>(m_pointLights.size()), MaxPointLights);
	shader->setUniform("pointLightCount", pointLightCount);
	for (int i = 0; i < pointLightCount; ++i)
	{
		const PointLight& pointLight = m_pointLights[i];
		const std::string uniformPrefix = "pointLights[" + std::to_string(i) + "].";
		shader->setUniform(uniformPrefix + "position", pointLight.position);
		shader->setUniform(uniformPrefix + "color", pointLight.color);
		shader->setUniform(uniformPrefix + "intensity", pointLight.intensity);
		shader->setUniform(uniformPrefix + "ambient", pointLight.ambient);
		shader->setUniform(uniformPrefix + "radius", pointLight.radius);
		shader->setUniform(uniformPrefix + "radiusFade", pointLight.radiusFade);
		shader->setUniform(uniformPrefix + "constant", pointLight.constant);
		shader->setUniform(uniformPrefix + "linear", pointLight.linear);
		shader->setUniform(uniformPrefix + "quadratic", pointLight.quadratic);
	}
}

DirectionalLight& LightingManager::SunLight()
{
	return m_sun;
}

const DirectionalLight& LightingManager::SunLight() const
{
	return m_sun;
}

bool LightingManager::ShadowsEnabled() const
{
	return m_shadowsEnabled;
}

void LightingManager::SetShadowsEnabled(bool enabled)
{
	m_shadowsEnabled = enabled;
}

PointLight& LightingManager::AddPointLight()
{
	if (m_pointLights.size() >= static_cast<std::size_t>(MaxPointLights))
	{
		return m_pointLights.back();
	}

	m_pointLights.emplace_back();
	return m_pointLights.back();
}

std::vector<PointLight>& LightingManager::PointLights()
{
	return m_pointLights;
}

const std::vector<PointLight>& LightingManager::PointLights() const
{
	return m_pointLights;
}

