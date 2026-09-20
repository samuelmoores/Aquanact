#include "Engine/Core/WeatherSystem.h"

#include "Engine/Core/Camera.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/Scene.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr std::size_t MaximumSplashCount = 512u;
	constexpr float SplashFrameCount = 5.0f;
	std::size_t BaseParticleCount(WeatherType type)
	{
		return type == WeatherType::Snow ? 500u : 3000u;
	}

	bool Different(glm::vec2 left, glm::vec2 right)
	{
		const glm::vec2 delta = left - right;
		return glm::dot(delta, delta) > 1e-8f;
	}
}

WeatherSystem::WeatherSystem()
{
	ApplyPreset(WeatherType::Rain);
	m_settings.enabled = false;
}

void WeatherSystem::ApplyPreset(WeatherType type)
{
	if (type == WeatherType::Count)
		type = WeatherType::Rain;

	const bool enabled = m_settings.enabled;
	m_settings = {};
	m_settings.enabled = enabled;
	m_settings.type = type;
	if (type == WeatherType::Snow)
	{
		m_settings.fallSpeed = 3.0f;
		m_settings.velocitySpread = 0.55f;
		m_settings.gravity = {0.0f, -0.1f, 0.0f};
		m_settings.spawnHeightAboveCamera = 30.0f;
		m_settings.fallDistanceBelowCamera = 15.0f;
		m_settings.coverageDepth = 100.0f;
		m_settings.minimumNearWidth = 140.0f;
		m_settings.startSize = 2.5f;
		m_settings.endSize = 1.5f;
		m_settings.startColor = {0.95f, 0.98f, 1.0f, 0.8f};
		m_settings.endColor = {0.8f, 0.9f, 1.0f, 0.0f};
	}
	m_poolDirty = true;
}

void WeatherSystem::SetSettings(const WeatherSettings& settings)
{
	WeatherSettings sanitized = settings;
	if (sanitized.type == WeatherType::Count)
		sanitized.type = WeatherType::Rain;
	sanitized.density = std::clamp(sanitized.density, 0.0f, MaximumDensity);
	sanitized.fallSpeed = std::max(0.0f, sanitized.fallSpeed);
	sanitized.velocitySpread = std::max(0.0f, sanitized.velocitySpread);
	sanitized.spawnHeightAboveCamera = std::max(0.0f, sanitized.spawnHeightAboveCamera);
	sanitized.fallDistanceBelowCamera = std::max(0.0f, sanitized.fallDistanceBelowCamera);
	sanitized.coverageDepth = std::max(1.0f, sanitized.coverageDepth);
	sanitized.minimumNearWidth = std::max(1.0f, sanitized.minimumNearWidth);
	sanitized.startSize = std::max(0.001f, sanitized.startSize);
	sanitized.endSize = std::max(0.0f, sanitized.endSize);

	const bool distributionChanged = sanitized.type != m_settings.type ||
		sanitized.fallSpeed != m_settings.fallSpeed ||
		Different(sanitized.windVelocity, m_settings.windVelocity) ||
		sanitized.velocitySpread != m_settings.velocitySpread ||
		sanitized.spawnHeightAboveCamera != m_settings.spawnHeightAboveCamera ||
		sanitized.fallDistanceBelowCamera != m_settings.fallDistanceBelowCamera ||
		sanitized.coverageDepth != m_settings.coverageDepth ||
		sanitized.minimumNearWidth != m_settings.minimumNearWidth;
	sanitized.splashSize = std::max(0.001f, sanitized.splashSize);
	sanitized.splashDensity = std::max(0.0f, sanitized.splashDensity);
	sanitized.splashFrameDuration = std::max(0.01f, sanitized.splashFrameDuration);
	sanitized.splashBrightness = std::max(0.0f, sanitized.splashBrightness);
	sanitized.splashOpacity = std::clamp(sanitized.splashOpacity, 0.0f, 1.0f);
	m_settings = sanitized;
	m_poolDirty = m_poolDirty || distributionChanged;
}

void WeatherSystem::Restart()
{
	m_poolDirty = true;
	m_splashes.clear();
	m_splashEmissionAccumulator = 0.0f;
}

void WeatherSystem::Clear()
{
	m_particles.clear();
	m_splashes.clear();
	m_splashEmissionAccumulator = 0.0f;
	ApplyPreset(WeatherType::Rain);
	m_settings.enabled = false;
	m_poolDirty = true;
}

void WeatherSystem::Update(float dt, const Camera& camera, const Scene& scene)
{
	UpdateCameraFrame(camera);
	RecalculateWorldVolume();
	if (!m_settings.enabled)
		return;

	SynchronizePool();
	for (std::size_t splashIndex = 0; splashIndex < m_splashes.size();)
	{
		ParticleInstance& splash = m_splashes[splashIndex];
		splash.age += std::max(dt, 0.0f);
		if (splash.age >= splash.lifetime)
		{
			m_splashes[splashIndex] = m_splashes.back();
			m_splashes.pop_back();
			continue;
		}
		++splashIndex;
	}

	if (m_settings.type == WeatherType::Rain && m_settings.splashesEnabled && dt > 0.0f)
	{
		// Splashes are intentionally decoupled from individual drops. This keeps
		// splash cost stable and distributes impacts across the visible ground.
		m_splashEmissionAccumulator += dt * m_settings.splashDensity;
		const std::size_t splashCount = static_cast<std::size_t>(m_splashEmissionAccumulator);
		m_splashEmissionAccumulator -= static_cast<float>(splashCount);
		for (std::size_t index = 0; index < splashCount; ++index)
			SpawnRandomSplash(scene);
	}

	for (ParticleInstance& particle : m_particles)
	{
		if (dt > 0.0f)
		{
			particle.velocity += m_settings.gravity * dt;
			particle.position += particle.velocity * dt;
		}

		const bool fellBelowVolume = particle.position.y < m_volumeMin.y;
		const bool outsideVolume = fellBelowVolume ||
			particle.position.x < m_volumeMin.x || particle.position.x > m_volumeMax.x ||
			particle.position.y > m_volumeMax.y ||
			particle.position.z < m_volumeMin.z || particle.position.z > m_volumeMax.z;
		if (outsideVolume)
		{
			// Natural rainfall re-enters above the view. Particles invalidated by
			// camera motion or rotation are redistributed through the volume so a
			// whole group cannot collect on a newly exposed spawn plane.
			ResetParticle(particle, !fellBelowVolume);
			continue;
		}

		const float verticalRange = std::max(m_volumeMax.y - m_volumeMin.y, 0.001f);
		const float progress = std::clamp(
			(m_volumeMax.y - particle.position.y) / verticalRange,
			0.0f, 1.0f);
		particle.age = progress;
		particle.lifetime = 1.0f;
		particle.color = glm::mix(m_settings.startColor, m_settings.endColor, progress);
		particle.size = glm::mix(m_settings.startSize, m_settings.endSize, progress);
	}
}

ParticleVisualShape WeatherSystem::VisualShape() const
{
	return m_settings.type == WeatherType::Rain
		? ParticleVisualShape::RainStreak
		: ParticleVisualShape::SoftCircle;
}

bool WeatherSystem::WorldBounds(glm::vec3& minimum, glm::vec3& maximum) const
{
	if (!m_settings.enabled || m_particles.empty())
		return false;
	minimum = m_boundsMin;
	maximum = m_boundsMax;
	return true;
}

std::size_t WeatherSystem::TargetParticleCount() const
{
	return static_cast<std::size_t>(std::ceil(
		static_cast<float>(BaseParticleCount(m_settings.type)) * m_settings.density));
}

void WeatherSystem::SynchronizePool()
{
	const std::size_t targetCount = TargetParticleCount();
	if (m_poolDirty)
	{
		m_particles.clear();
		m_particles.resize(targetCount);
		for (ParticleInstance& particle : m_particles)
			ResetParticle(particle, true);
		m_poolDirty = false;
		return;
	}

	const std::size_t previousCount = m_particles.size();
	m_particles.resize(targetCount);
	for (std::size_t index = previousCount; index < targetCount; ++index)
		ResetParticle(m_particles[index], true);
}

void WeatherSystem::ResetParticle(ParticleInstance& particle, bool prewarm)
{
	std::uniform_real_distribution<float> xDistribution(m_volumeMin.x, m_volumeMax.x);
	std::uniform_real_distribution<float> zDistribution(m_volumeMin.z, m_volumeMax.z);
	std::uniform_real_distribution<float> spreadDistribution(
		-m_settings.velocitySpread, m_settings.velocitySpread);
	std::uniform_real_distribution<float> variationDistribution(0.0f, 1.0f);

	glm::vec3 worldPosition{
		xDistribution(m_random),
		m_volumeMax.y,
		zDistribution(m_random)};
	if (prewarm)
	{
		std::uniform_real_distribution<float> heightDistribution(
			m_volumeMin.y, m_volumeMax.y);
		worldPosition.y = heightDistribution(m_random);
	}
	else
	{
		// Give newly recycled particles a spawn band instead of a shared Y
		// coordinate. The band lies in the padding above the visible frustum.
		const float spawnBandHeight = std::max(m_settings.spawnHeightAboveCamera, 1.0f);
		std::uniform_real_distribution<float> spawnHeightDistribution(
			std::max(m_volumeMin.y, m_volumeMax.y - spawnBandHeight), m_volumeMax.y);
		worldPosition.y = spawnHeightDistribution(m_random);
	}

	particle = {};
	particle.position = worldPosition;
	particle.velocity = {
		m_settings.windVelocity.x + spreadDistribution(m_random),
		-m_settings.fallSpeed + spreadDistribution(m_random),
		m_settings.windVelocity.y + spreadDistribution(m_random)};
	particle.shapeVariation = variationDistribution(m_random);

	const float verticalRange = std::max(m_volumeMax.y - m_volumeMin.y, 0.001f);
	const float progress = std::clamp(
		(m_volumeMax.y - worldPosition.y) / verticalRange,
		0.0f, 1.0f);
	particle.age = progress;
	particle.lifetime = 1.0f;
	particle.color = glm::mix(m_settings.startColor, m_settings.endColor, progress);
	particle.size = glm::mix(m_settings.startSize, m_settings.endSize, progress);
}

void WeatherSystem::SpawnSplash(const glm::vec3& position)
{
	ParticleInstance splash;
	splash.position = position;
	splash.color = {
		m_settings.startColor.r, m_settings.startColor.g, m_settings.startColor.b,
		1.0f};
	splash.size = m_settings.splashSize;
	splash.age = 0.0f;
	splash.lifetime = SplashFrameCount * std::max(m_settings.splashFrameDuration, 0.01f);
	splash.shapeVariation = 0.5f;

	// Do not replace an active splash when the pool is full. Replacing one
	// would repeatedly reset older animations to frame zero at high density or
	// with long frame durations, making the later sheet frames hard to see.
	if (m_splashes.size() < MaximumSplashCount)
		m_splashes.push_back(splash);
}

void WeatherSystem::SpawnRandomSplash(const Scene& scene)
{
	struct Surface
	{
		Entity* object;
		glm::vec3 minimum;
		glm::vec3 maximum;
		float weight;
	};

	std::vector<Surface> surfaces;
	float totalWeight = 0.0f;
	for (const auto& object : scene.Objects())
	{
		if (!object || !object->GetMesh() ||
			std::find(m_settings.groundEntityIds.begin(), m_settings.groundEntityIds.end(), object->Id()) ==
				m_settings.groundEntityIds.end())
			continue;

		glm::vec3 minimum(0.0f);
		glm::vec3 maximum(0.0f);
		if (!object->WorldAABB(minimum, maximum))
			continue;

		const glm::vec2 extent(maximum.x - minimum.x, maximum.z - minimum.z);
		const float area = extent.x * extent.y;
		if (area <= 0.0001f)
			continue;

		surfaces.push_back({object.get(), minimum, maximum, area});
		totalWeight += area;
	}

	if (surfaces.empty() || totalWeight <= 0.0f)
		return;

	std::uniform_real_distribution<float> selection(0.0f, totalWeight);
	float target = selection(m_random);
	for (const Surface& surface : surfaces)
	{
		if (target > surface.weight)
		{
			target -= surface.weight;
			continue;
		}

		std::uniform_real_distribution<float> xDistribution(
			surface.minimum.x, surface.maximum.x);
		std::uniform_real_distribution<float> zDistribution(
			surface.minimum.z, surface.maximum.z);

		if (surface.object->GetPhysicsColliderShape() == PhysicsColliderShape::Convex)
		{
			// The AABB is only used to choose a candidate location. Resolve its
			// height against the actual convex collider so ramps and staircases do
			// not receive splashes on the collider's box ceiling.
			constexpr int SurfaceSamples = 12;
			for (int sample = 0; sample < SurfaceSamples; ++sample)
			{
				const glm::vec2 horizontal(xDistribution(m_random), zDistribution(m_random));
				glm::vec3 surfacePoint;
				if (PhysicsWorld::Instance().FindConvexSurfacePoint(
					*surface.object, horizontal, surfacePoint))
				{
					SpawnSplash(surfacePoint + glm::vec3(0.0f, 0.01f, 0.0f));
					return;
				}
			}
			return;
		}

		SpawnSplash({xDistribution(m_random), surface.maximum.y + 0.01f, zDistribution(m_random)});
		return;
	}
}

void WeatherSystem::UpdateCameraFrame(const Camera& camera)
{
	m_cameraPosition = camera.GetPosition();
	const glm::vec3 cameraFacing = camera.GetFacing();
	if (glm::length(cameraFacing) > 1e-6f)
		m_cameraForward = glm::normalize(cameraFacing);

	const glm::mat4 inverseView = glm::inverse(camera.GetViewMatrix());
	const glm::vec3 cameraRight(inverseView[0]);
	const glm::vec3 cameraUp(inverseView[1]);
	if (glm::length(cameraRight) > 1e-6f)
		m_cameraRight = glm::normalize(cameraRight);
	if (glm::length(cameraUp) > 1e-6f)
		m_cameraUp = glm::normalize(cameraUp);

	const glm::mat4 projection = camera.GetProjectionMatrix();
	const float horizontalProjectionScale = std::abs(projection[0][0]);
	if (std::isfinite(horizontalProjectionScale) && horizontalProjectionScale > 1e-6f)
		m_tanHalfHorizontalFov = std::clamp(1.0f / horizontalProjectionScale, 0.01f, 10.0f);
	const float verticalProjectionScale = std::abs(projection[1][1]);
	if (std::isfinite(verticalProjectionScale) && verticalProjectionScale > 1e-6f)
		m_tanHalfVerticalFov = std::clamp(1.0f / verticalProjectionScale, 0.01f, 10.0f);
}

void WeatherSystem::RecalculateWorldVolume()
{
	// Bound the actual camera frustum in world space, then pad that AABB. This
	// remains well-defined when the view points vertically, unlike a volume
	// parameterized by horizontal camera depth.
	const float depth = m_settings.coverageDepth;
	const glm::vec3 farCenter = m_cameraPosition + m_cameraForward * depth;
	const glm::vec3 farRight = m_cameraRight * (depth * m_tanHalfHorizontalFov);
	const glm::vec3 farUp = m_cameraUp * (depth * m_tanHalfVerticalFov);
	const glm::vec3 nearExtent(m_settings.minimumNearWidth * 0.5f, 0.0f,
		m_settings.minimumNearWidth * 0.5f);
	glm::vec3 frustumMin = m_cameraPosition - nearExtent;
	glm::vec3 frustumMax = m_cameraPosition + nearExtent;
	for (int rightIndex = 0; rightIndex < 2; ++rightIndex)
	{
		const float rightSign = rightIndex == 0 ? -1.0f : 1.0f;
		for (int upIndex = 0; upIndex < 2; ++upIndex)
		{
			const float upSign = upIndex == 0 ? -1.0f : 1.0f;
			const glm::vec3 corner = farCenter + farRight * rightSign + farUp * upSign;
			frustumMin = glm::min(frustumMin, corner);
			frustumMax = glm::max(frustumMax, corner);
		}
	}

	const float horizontalPadding = EdgePadding();
	const float spawnPadding = std::max(m_settings.spawnHeightAboveCamera, 1.0f);
	m_volumeMin = frustumMin - glm::vec3(
		horizontalPadding, m_settings.fallDistanceBelowCamera, horizontalPadding);
	m_volumeMax = frustumMax + glm::vec3(
		horizontalPadding, spawnPadding, horizontalPadding);

	const float maximumSize = std::max(m_settings.startSize, m_settings.endSize);
	m_boundsMin = m_volumeMin - glm::vec3(maximumSize);
	m_boundsMax = m_volumeMax + glm::vec3(maximumSize);
}

float WeatherSystem::EdgePadding() const
{
	return std::max(10.0f, m_settings.coverageDepth * 0.15f);
}
