#include "LightingManager.h"

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
}
