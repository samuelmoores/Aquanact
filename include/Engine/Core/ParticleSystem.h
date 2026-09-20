#pragma once

#include "Engine/Core/Component.h"

#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

class Entity;

enum class ParticleEmissionShape
{
	Point = 0,
	Sphere = 1,
	Box = 2
};

enum class ParticleBlendMode
{
	Additive = 0,
	Alpha = 1
};

enum class ParticleVisualShape
{
	SoftCircle = 0,
	RainStreak = 1,
	SplashSpriteSheet = 2
};

enum class ParticleSimulationSpace
{
	Local = 0,
	World = 1
};

enum class ParticlePreset
{
	Custom = 0,
	TorchFlame,
	Embers,
	BlueMagic,
	HealingMotes,
	PoisonGlow,
	ElectricalSparks,
	FireFountain,
	Smoke,
	Explosion,
	Trail,
	Count
};

#define PARTICLE_SYSTEM_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
	BIND_VALUE(m_emissionRate) \
	BIND_VALUE(m_maxParticles) \
	BIND_VALUE(m_lifetime) \
	BIND_VALUE(m_particleSize) \
	BIND_VALUE(m_endParticleSize) \
	BIND_VALUE(m_velocitySpread) \
	BIND_VALUE(m_radialSpeed) \
	BIND_VALUE(m_emissionRadius)

struct ParticleInstance
{
	glm::vec3 position{0.0f};
	glm::vec3 velocity{0.0f};
	glm::vec4 color{1.0f};
	float size = 0.25f;
	float age = 0.0f;
	float lifetime = 1.0f;
	float shapeVariation = 0.5f;
};

class ParticleSystem final : public Component
{
public:
	const char* Name() const override { return "ParticleSystem"; }
	int ExecutionOrder() const override { return 100; }
	void startUp(Entity&) override;
	void Update(Entity&, float dt) override;

	void Burst(std::size_t count);
	void Restart();
	void ApplyPreset(ParticlePreset preset);
	ParticlePreset Preset() const { return m_preset; }
	ParticleVisualShape VisualShape() const { return ParticleVisualShape::SoftCircle; }
	// Restores the saved labels after their individual editable properties load.
	void RestorePreset(ParticlePreset preset);
	const std::vector<ParticleInstance>& Particles() const { return m_particles; }
	bool SimulationBounds(glm::vec3& minimum, glm::vec3& maximum) const;

	float EmissionRate() const { return m_emissionRate; }
	void SetEmissionRate(float value) { m_emissionRate = value < 0.0f ? 0.0f : value; }
	int MaxParticles() const { return m_maxParticles; }
	void SetMaxParticles(int value)
	{
		m_maxParticles = value < 1 ? 1 : value;
		m_particles.reserve(static_cast<std::size_t>(m_maxParticles));
		TrimToLimit();
	}
	float Lifetime() const { return m_lifetime; }
	void SetLifetime(float value) { m_lifetime = value < 0.01f ? 0.01f : value; }
	float ParticleSize() const { return m_particleSize; }
	void SetParticleSize(float value) { m_particleSize = value < 0.001f ? 0.001f : value; }
	float EndParticleSize() const { return m_endParticleSize; }
	void SetEndParticleSize(float value) { m_endParticleSize = value < 0.0f ? 0.0f : value; }
	glm::vec3 InitialVelocity() const { return m_initialVelocity; }
	void SetInitialVelocity(glm::vec3 value) { m_initialVelocity = value; }
	float VelocitySpread() const { return m_velocitySpread; }
	void SetVelocitySpread(float value) { m_velocitySpread = value < 0.0f ? 0.0f : value; }
	float RadialSpeed() const { return m_radialSpeed; }
	void SetRadialSpeed(float value) { m_radialSpeed = value < 0.0f ? 0.0f : value; }
	glm::vec3 Gravity() const { return m_gravity; }
	void SetGravity(glm::vec3 value) { m_gravity = value; }
	glm::vec4 StartColor() const { return m_startColor; }
	void SetStartColor(glm::vec4 value) { m_startColor = value; }
	glm::vec4 EndColor() const { return m_endColor; }
	void SetEndColor(glm::vec4 value) { m_endColor = value; }
	bool Looping() const { return m_looping; }
	void SetLooping(bool value) { m_looping = value; }
	ParticleEmissionShape EmissionShape() const { return m_emissionShape; }
	void SetEmissionShape(ParticleEmissionShape value) { m_emissionShape = value; }
	float EmissionRadius() const { return m_emissionRadius; }
	void SetEmissionRadius(float value) { m_emissionRadius = value < 0.0f ? 0.0f : value; }
	glm::vec3 EmissionBoxExtents() const { return m_emissionBoxExtents; }
	void SetEmissionBoxExtents(glm::vec3 value) { m_emissionBoxExtents = glm::max(value, glm::vec3(0.0f)); }
	ParticleBlendMode BlendMode() const { return m_blendMode; }
	void SetBlendMode(ParticleBlendMode value) { m_blendMode = value; }
	ParticleSimulationSpace SimulationSpace() const { return m_simulationSpace; }
	void SetSimulationSpace(ParticleSimulationSpace value)
	{
		if (m_simulationSpace == value) return;
		m_simulationSpace = value;
		Restart();
	}

	AQUA_DECLARE_BINDABLES(PARTICLE_SYSTEM_BINDABLES)

private:
	void SpawnOne();
	void TrimToLimit();
	void ResetBounds();
	void ExpandBounds(const ParticleInstance& particle);
	void RecalculateBounds();
	glm::vec3 RandomInUnitSphere();
	glm::vec3 RandomUnitVector();
	glm::vec3 SpawnPosition();

	std::vector<ParticleInstance> m_particles;
	glm::vec3 m_boundsMin{0.0f};
	glm::vec3 m_boundsMax{0.0f};
	bool m_hasBounds = false;
	float m_emissionAccumulator = 0.0f;
	float m_emissionRate = 12.0f;
	int m_maxParticles = 128;
	float m_lifetime = 1.0f;
	float m_particleSize = 12.5f;
	float m_endParticleSize = 12.5f;
	glm::vec3 m_initialVelocity{0.0f, 1.0f, 0.0f};
	float m_velocitySpread = 0.35f;
	float m_radialSpeed = 0.0f;
	glm::vec3 m_gravity{0.0f, -1.5f, 0.0f};
	glm::vec4 m_startColor{1.0f, 0.55f, 0.1f, 1.0f};
	glm::vec4 m_endColor{1.0f, 0.05f, 0.0f, 0.0f};
	bool m_looping = true;
	ParticleEmissionShape m_emissionShape = ParticleEmissionShape::Point;
	float m_emissionRadius = 1.0f;
	glm::vec3 m_emissionBoxExtents{1.0f};
	ParticleBlendMode m_blendMode = ParticleBlendMode::Additive;
	ParticleSimulationSpace m_simulationSpace = ParticleSimulationSpace::Local;
	ParticlePreset m_preset = ParticlePreset::Custom;
	std::mt19937 m_random{0xA0A1234u};
};
