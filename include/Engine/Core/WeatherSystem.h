#pragma once

#include "Engine/Core/ParticleSystem.h"

#include <glm/glm.hpp>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

class Camera;
class Scene;

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
	bool splashesEnabled = true;
	float groundHeight = 0.0f;
	float splashSize = 0.18f;
	float splashDensity = 18.0f;
	float splashFrameDuration = 1.0f;
	float splashBrightness = 1.0f;
	float splashOpacity = 1.0f;
	std::string splashTexturePath = "rain splash.png";
	std::vector<unsigned int> groundEntityIds;
	glm::vec4 startColor{0.55f, 0.75f, 1.0f, 0.45f};
	glm::vec4 endColor{0.35f, 0.55f, 1.0f, 0.0f};
};

class WeatherSystem
{
public:
	static constexpr float MaximumDensity = 20.0f;

	WeatherSystem();

	void Update(float dt, const Camera& camera, const Scene& scene);
	void ApplyPreset(WeatherType type);
	void SetSettings(const WeatherSettings& settings);
	void Restart();
	void Clear();

	const WeatherSettings& Settings() const { return m_settings; }
	const std::vector<ParticleInstance>& Particles() const { return m_particles; }
	const std::vector<ParticleInstance>& SplashParticles() const { return m_splashes; }
	ParticleVisualShape VisualShape() const;
	bool WorldBounds(glm::vec3& minimum, glm::vec3& maximum) const;

private:
	std::size_t TargetParticleCount() const;
	void SynchronizePool();
	void ResetParticle(ParticleInstance& particle, bool prewarm);
	void SpawnSplash(const glm::vec3& position);
	void SpawnRandomSplash(const Scene& scene);
	void UpdateCameraFrame(const Camera& camera);
	void RecalculateWorldVolume();
	float EdgePadding() const;

	WeatherSettings m_settings;
	std::vector<ParticleInstance> m_particles;
	std::vector<ParticleInstance> m_splashes;
	float m_splashEmissionAccumulator = 0.0f;
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
