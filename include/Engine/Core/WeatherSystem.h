#pragma once

#include "Engine/Core/ParticleSystem.h"

#include <glm/glm.hpp>
#include <cstddef>
#include <random>
#include <vector>

class Camera;

enum class WeatherType
{
	Rain = 0,
	Snow = 1,
	Count
};

struct WeatherSettings
{
	bool enabled = false;
	WeatherType type = WeatherType::Rain;
	float density = 1.0f;
	float fallSpeed = 25.0f;
	glm::vec2 windVelocity{0.0f};
	float velocitySpread = 0.35f;
	glm::vec3 gravity{0.0f, -9.0f, 0.0f};
	float spawnHeightAboveCamera = 40.0f;
	float fallDistanceBelowCamera = 20.0f;
	float coverageDepth = 120.0f;
	float minimumNearWidth = 160.0f;
	float startSize = 1.0f;
	float endSize = 1.0f;
	glm::vec4 startColor{0.55f, 0.75f, 1.0f, 0.45f};
	glm::vec4 endColor{0.35f, 0.55f, 1.0f, 0.0f};
};

class WeatherSystem
{
public:
	static constexpr float MaximumDensity = 20.0f;

	WeatherSystem();

	void Update(float dt, const Camera& camera);
	void ApplyPreset(WeatherType type);
	void SetSettings(const WeatherSettings& settings);
	void Restart();
	void Clear();

	const WeatherSettings& Settings() const { return m_settings; }
	const std::vector<ParticleInstance>& Particles() const { return m_particles; }
	ParticleVisualShape VisualShape() const;
	bool WorldBounds(glm::vec3& minimum, glm::vec3& maximum) const;

private:
	std::size_t TargetParticleCount() const;
	void SynchronizePool();
	void ResetParticle(ParticleInstance& particle, bool prewarm);
	void UpdateCameraFrame(const Camera& camera);
	void RecalculateWorldVolume();
	float EdgePadding() const;

	WeatherSettings m_settings;
	std::vector<ParticleInstance> m_particles;
	glm::vec3 m_boundsMin{0.0f};
	glm::vec3 m_boundsMax{0.0f};
	glm::vec3 m_volumeMin{0.0f};
	glm::vec3 m_volumeMax{0.0f};
	glm::vec3 m_cameraPosition{0.0f};
	glm::vec3 m_cameraRight{1.0f, 0.0f, 0.0f};
	glm::vec3 m_cameraUp{0.0f, 1.0f, 0.0f};
	glm::vec3 m_cameraForward{0.0f, 0.0f, -1.0f};
	float m_tanHalfHorizontalFov = 1.0f;
	float m_tanHalfVerticalFov = 1.0f;
	bool m_poolDirty = true;
	std::mt19937 m_random{0x57EA7E12u};
};
