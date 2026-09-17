#include "Engine/Core/ParticleSystem.h"

#include "Engine/Core/Entity.h"

#include <algorithm>
#include <cmath>
#include <random>

void ParticleSystem::startUp(Entity&)
{
	m_particles.reserve(static_cast<std::size_t>(m_maxParticles));
	Restart();
}

void ParticleSystem::Update(Entity&, float dt)
{
	if (dt <= 0.0f) return;

	ResetBounds();
	std::size_t liveParticleCount = 0;
	for (std::size_t particleIndex = 0; particleIndex < m_particles.size(); ++particleIndex)
	{
		ParticleInstance& particle = m_particles[particleIndex];
		particle.age += dt;
		if (particle.age >= particle.lifetime)
			continue;

		particle.velocity += m_gravity * dt;
		particle.position += particle.velocity * dt;
		const float t = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
		particle.color = glm::mix(m_startColor, m_endColor, t);
		particle.size = glm::mix(m_particleSize, m_endParticleSize, t);
		ExpandBounds(particle);

		if (liveParticleCount != particleIndex)
			m_particles[liveParticleCount] = particle;
		++liveParticleCount;
	}
	m_particles.resize(liveParticleCount);

	if (!m_looping) return;
	m_emissionAccumulator += dt * m_emissionRate;
	const std::size_t spawnCount = static_cast<std::size_t>(m_emissionAccumulator);
	m_emissionAccumulator -= static_cast<float>(spawnCount);
	for (std::size_t i = 0; i < spawnCount && m_particles.size() < static_cast<std::size_t>(m_maxParticles); ++i)
		SpawnOne();
}

void ParticleSystem::Burst(std::size_t count)
{
	for (std::size_t i = 0; i < count && m_particles.size() < static_cast<std::size_t>(m_maxParticles); ++i)
		SpawnOne();
}

void ParticleSystem::Restart()
{
	m_particles.clear();
	m_emissionAccumulator = 0.0f;
	ResetBounds();
}

bool ParticleSystem::SimulationBounds(glm::vec3& minimum, glm::vec3& maximum) const
{
	if (!m_hasBounds) return false;
	minimum = m_boundsMin;
	maximum = m_boundsMax;
	return true;
}

void ParticleSystem::RestorePreset(ParticlePreset preset)
{
	if (preset == ParticlePreset::Count)
		preset = ParticlePreset::Custom;
	m_preset = preset;
}

void ParticleSystem::ApplyPreset(ParticlePreset preset)
{
	if (preset == ParticlePreset::Custom || preset == ParticlePreset::Count)
	{
		m_preset = ParticlePreset::Custom;
		return;
	}

	// Presets only populate regular emitter properties. Every value remains
	// editable after applying a preset and is serialized normally.
	m_emissionShape = ParticleEmissionShape::Point;
	m_emissionRadius = 1.0f;
	m_emissionBoxExtents = glm::vec3(1.0f);
	m_blendMode = ParticleBlendMode::Additive;
	m_simulationSpace = ParticleSimulationSpace::Local;
	m_radialSpeed = 0.0f;

	switch (preset)
	{
	case ParticlePreset::Custom:
	case ParticlePreset::Count:
		return;
	case ParticlePreset::TorchFlame:
		m_emissionRate = 30.0f; m_maxParticles = 96; m_lifetime = 0.7f;
		m_particleSize = 9.0f; m_endParticleSize = 2.0f;
		m_initialVelocity = {0.0f, 1.2f, 0.0f}; m_velocitySpread = 0.45f; m_gravity = {0.0f, 0.4f, 0.0f};
		m_startColor = {1.0f, 0.75f, 0.15f, 1.0f}; m_endColor = {1.0f, 0.05f, 0.0f, 0.0f}; m_looping = true;
		break;
	case ParticlePreset::Embers:
		m_emissionRate = 6.0f; m_maxParticles = 64; m_lifetime = 3.0f;
		m_particleSize = 3.0f; m_endParticleSize = 0.5f;
		m_initialVelocity = {0.0f, 0.5f, 0.0f}; m_velocitySpread = 0.2f; m_gravity = {0.0f, 0.08f, 0.0f};
		m_startColor = {1.0f, 0.45f, 0.05f, 0.9f}; m_endColor = {0.5f, 0.02f, 0.0f, 0.0f}; m_looping = true;
		m_simulationSpace = ParticleSimulationSpace::World;
		break;
	case ParticlePreset::BlueMagic:
		m_emissionRate = 18.0f; m_maxParticles = 128; m_lifetime = 1.8f;
		m_particleSize = 8.0f; m_endParticleSize = 2.0f;
		m_initialVelocity = {0.0f, 0.25f, 0.0f}; m_velocitySpread = 0.3f; m_gravity = {0.0f, 0.0f, 0.0f};
		m_startColor = {0.1f, 0.9f, 1.0f, 1.0f}; m_endColor = {0.35f, 0.05f, 1.0f, 0.0f}; m_looping = true;
		m_emissionShape = ParticleEmissionShape::Sphere; m_emissionRadius = 2.0f;
		break;
	case ParticlePreset::HealingMotes:
		m_emissionRate = 10.0f; m_maxParticles = 80; m_lifetime = 2.2f;
		m_particleSize = 5.0f; m_endParticleSize = 1.0f;
		m_initialVelocity = {0.0f, 0.7f, 0.0f}; m_velocitySpread = 0.2f; m_gravity = {0.0f, 0.05f, 0.0f};
		m_startColor = {0.65f, 1.0f, 0.65f, 0.9f}; m_endColor = {0.0f, 0.7f, 0.25f, 0.0f}; m_looping = true;
		m_emissionShape = ParticleEmissionShape::Sphere; m_emissionRadius = 3.0f;
		break;
	case ParticlePreset::PoisonGlow:
		m_emissionRate = 14.0f; m_maxParticles = 96; m_lifetime = 2.5f;
		m_particleSize = 10.0f; m_endParticleSize = 16.0f;
		m_initialVelocity = {0.0f, 0.3f, 0.0f}; m_velocitySpread = 0.18f; m_gravity = {0.0f, 0.0f, 0.0f};
		m_startColor = {0.65f, 1.0f, 0.05f, 0.45f}; m_endColor = {0.05f, 0.2f, 0.0f, 0.0f}; m_looping = true;
		m_emissionShape = ParticleEmissionShape::Sphere; m_emissionRadius = 2.5f;
		break;
	case ParticlePreset::ElectricalSparks:
		m_emissionRate = 0.0f; m_maxParticles = 32; m_lifetime = 0.25f;
		m_particleSize = 4.0f; m_endParticleSize = 0.0f;
		m_initialVelocity = {0.0f, 0.0f, 0.0f}; m_velocitySpread = 0.4f; m_radialSpeed = 4.5f; m_gravity = {0.0f, -4.0f, 0.0f};
		m_startColor = {0.85f, 0.95f, 1.0f, 1.0f}; m_endColor = {0.05f, 0.25f, 1.0f, 0.0f}; m_looping = false;
		m_simulationSpace = ParticleSimulationSpace::World;
		break;
	case ParticlePreset::FireFountain:
		m_emissionRate = 45.0f; m_maxParticles = 160; m_lifetime = 1.4f;
		m_particleSize = 6.0f; m_endParticleSize = 1.0f;
		m_initialVelocity = {0.0f, 2.5f, 0.0f}; m_velocitySpread = 0.55f; m_gravity = {0.0f, -2.5f, 0.0f};
		m_startColor = {1.0f, 0.8f, 0.15f, 1.0f}; m_endColor = {1.0f, 0.03f, 0.0f, 0.0f}; m_looping = true;
		break;
	case ParticlePreset::Smoke:
		m_emissionRate = 10.0f; m_maxParticles = 96; m_lifetime = 3.0f;
		m_particleSize = 8.0f; m_endParticleSize = 24.0f;
		m_initialVelocity = {0.0f, 0.4f, 0.0f}; m_velocitySpread = 0.2f; m_gravity = {0.0f, 0.08f, 0.0f};
		m_startColor = {0.3f, 0.3f, 0.3f, 0.3f}; m_endColor = {0.08f, 0.08f, 0.08f, 0.0f}; m_looping = true;
		m_emissionShape = ParticleEmissionShape::Sphere; m_emissionRadius = 1.5f;
		m_blendMode = ParticleBlendMode::Alpha; m_simulationSpace = ParticleSimulationSpace::World;
		break;
	case ParticlePreset::Explosion:
		m_emissionRate = 0.0f; m_maxParticles = 128; m_lifetime = 0.65f;
		m_particleSize = 7.0f; m_endParticleSize = 15.0f;
		m_initialVelocity = {0.0f, 0.0f, 0.0f}; m_velocitySpread = 0.8f; m_radialSpeed = 6.0f; m_gravity = {0.0f, -1.5f, 0.0f};
		m_startColor = {1.0f, 0.9f, 0.3f, 1.0f}; m_endColor = {1.0f, 0.02f, 0.0f, 0.0f}; m_looping = false;
		m_emissionShape = ParticleEmissionShape::Sphere; m_emissionRadius = 0.25f;
		m_simulationSpace = ParticleSimulationSpace::World;
		break;
	case ParticlePreset::Trail:
		m_emissionRate = 45.0f; m_maxParticles = 128; m_lifetime = 0.55f;
		m_particleSize = 4.0f; m_endParticleSize = 0.5f;
		m_initialVelocity = {0.0f, 0.0f, 0.0f}; m_velocitySpread = 0.1f; m_gravity = {0.0f, 0.0f, 0.0f};
		m_startColor = {0.2f, 0.7f, 1.0f, 0.8f}; m_endColor = {0.05f, 0.1f, 1.0f, 0.0f}; m_looping = true;
		m_simulationSpace = ParticleSimulationSpace::World;
		break;
	}

	m_preset = preset;
	m_particles.reserve(static_cast<std::size_t>(m_maxParticles));
	Restart();
}

void ParticleSystem::SpawnOne()
{
	std::uniform_real_distribution<float> spread(-m_velocitySpread, m_velocitySpread);
	ParticleInstance particle;
	particle.position = SpawnPosition();
	particle.velocity = m_initialVelocity +
		glm::vec3(spread(m_random), spread(m_random), spread(m_random));
	if (m_radialSpeed > 0.0f)
	{
		const glm::vec3 radialDirection = glm::dot(particle.position, particle.position) > 1e-6f
			? glm::normalize(particle.position)
			: RandomUnitVector();
		particle.velocity += radialDirection * m_radialSpeed;
	}
	particle.color = m_startColor;
	particle.size = m_particleSize;
	particle.lifetime = m_lifetime;
	std::uniform_real_distribution<float> variation(0.0f, 1.0f);
	particle.shapeVariation = variation(m_random);
	if (m_simulationSpace == ParticleSimulationSpace::World && Owner())
	{
		const glm::mat4 emitterTransform = Owner()->BuildModelMatrix();
		particle.position = glm::vec3(emitterTransform * glm::vec4(particle.position, 1.0f));
		particle.velocity = glm::mat3(emitterTransform) * particle.velocity;
	}
	m_particles.push_back(particle);
	ExpandBounds(m_particles.back());
}

void ParticleSystem::TrimToLimit()
{
	if (m_particles.size() > static_cast<std::size_t>(m_maxParticles))
	{
		m_particles.resize(static_cast<std::size_t>(m_maxParticles));
		RecalculateBounds();
	}
}

void ParticleSystem::ResetBounds()
{
	m_boundsMin = glm::vec3(0.0f);
	m_boundsMax = glm::vec3(0.0f);
	m_hasBounds = false;
}

void ParticleSystem::ExpandBounds(const ParticleInstance& particle)
{
	const glm::vec3 extent(particle.size);
	const glm::vec3 particleMin = particle.position - extent;
	const glm::vec3 particleMax = particle.position + extent;
	if (!m_hasBounds)
	{
		m_boundsMin = particleMin;
		m_boundsMax = particleMax;
		m_hasBounds = true;
		return;
	}

	m_boundsMin = glm::min(m_boundsMin, particleMin);
	m_boundsMax = glm::max(m_boundsMax, particleMax);
}

void ParticleSystem::RecalculateBounds()
{
	ResetBounds();
	for (const ParticleInstance& particle : m_particles)
		ExpandBounds(particle);
}

glm::vec3 ParticleSystem::RandomInUnitSphere()
{
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	for (;;)
	{
		const glm::vec3 value(unit(m_random), unit(m_random), unit(m_random));
		const float lengthSquared = glm::dot(value, value);
		if (lengthSquared <= 1.0f)
			return value;
	}
}

glm::vec3 ParticleSystem::RandomUnitVector()
{
	for (;;)
	{
		const glm::vec3 value = RandomInUnitSphere();
		const float lengthSquared = glm::dot(value, value);
		if (lengthSquared > 1e-6f)
			return value / std::sqrt(lengthSquared);
	}
}

glm::vec3 ParticleSystem::SpawnPosition()
{
	switch (m_emissionShape)
	{
	case ParticleEmissionShape::Sphere:
		return RandomInUnitSphere() * m_emissionRadius;
	case ParticleEmissionShape::Box:
	{
		std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
		return glm::vec3(unit(m_random), unit(m_random), unit(m_random)) * m_emissionBoxExtents;
	}
	case ParticleEmissionShape::Point:
	default:
		return glm::vec3(0.0f);
	}
}
