#include "Engine/Core/PathedCamera.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Entity.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

void PathedCamera::startUp()
{
	m_window = Root::Current().WindowRef().GLFW();
	RebuildView();
}

void PathedCamera::shutDown()
{
	m_window = nullptr;
}

glm::mat4 PathedCamera::GetProjectionMatrix() const
{
	int width = 1;
	int height = 1;
	if (m_window)
	{
		glfwGetWindowSize(m_window, &width, &height);
	}
	if (height <= 0) height = 1;
	return glm::perspective(glm::radians(m_fieldOfView),
		static_cast<float>(width) / static_cast<float>(height),
		m_nearPlane, m_farPlane);
}

glm::mat4 PathedCamera::GetViewMatrix() const { return m_viewMatrix; }
glm::vec3 PathedCamera::GetPosition() const { return m_position; }
glm::vec3 PathedCamera::GetFacing() const { return m_facing; }

void PathedCamera::SetPose(const glm::vec3& position, const glm::vec3& facing)
{
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return;
	m_position = position;
	if (std::isfinite(facing.x) && std::isfinite(facing.y) && std::isfinite(facing.z) &&
		glm::dot(facing, facing) > 1e-8f)
		m_facing = glm::normalize(facing);
	else
		m_facing = {0.0f, 0.0f, 1.0f};
	m_up = std::abs(glm::dot(m_facing, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.999f
		? glm::vec3(0.0f, 0.0f, 1.0f)
		: glm::vec3(0.0f, 1.0f, 0.0f);
	RebuildView();
}

void PathedCamera::SetPath(const CameraPathData& path)
{
	m_path = IsValidCameraPath(path) ? path : CameraPathData{};
	m_desiredFollowProgress = 0.0f;
	m_previousPlayerProgress = m_playerProgress;
	if (!m_path.points.empty())
	{
		m_position = m_path.points.front().position;
		RebuildView();
	}
}

void PathedCamera::SetTarget(Entity* target)
{
	if (m_target == target)
	{
		return;
	}
	m_target = target;
	FaceTarget();
	RebuildView();
}

void PathedCamera::SetPlayerProgress(float progress)
{
	if (std::isfinite(progress))
	{
		const float nextProgress = glm::clamp(progress, 0.0f, 1.0f);
		const float delta = nextProgress - m_playerProgress;
		if (std::abs(delta) > 0.001f)
			m_playerPathDirection = delta > 0.0f ? 1.0f : -1.0f;
		m_previousPlayerProgress = m_playerProgress;
		m_playerProgress = nextProgress;
	}
}

float PathedCamera::ClosestPathDistance(const glm::vec3& worldPosition) const
{
	return ProjectOntoCameraPath(m_path, worldPosition, m_pathSamplesPerSegment).normalizedProgress;
}

void PathedCamera::SetPathSamplesPerSegment(int samples)
{
	m_pathSamplesPerSegment = std::clamp(samples, 4, 256);
}

void PathedCamera::Update(float deltaTime)
{
	const float blend = 1.0f - std::exp(-std::max(0.0f, m_followSharpness) * std::max(0.0f, deltaTime));
	if (m_path.points.empty())
	{
		m_desiredFollowProgress = 0.0f;
		m_desiredFollowPosition = m_position;
		m_desiredTargetDistance = 0.0f;
		m_actualTargetDistance = 0.0f;
		return;
	}

	if (m_path.points.size() == 1)
	{
		m_desiredFollowProgress = 0.0f;
		m_desiredFollowPosition = m_path.points.front().position;
		m_position = glm::mix(m_position, m_desiredFollowPosition, blend);
		if (m_target)
		{
			m_desiredTargetDistance = glm::length(m_desiredFollowPosition - m_target->WorldCenterPosition());
			m_actualTargetDistance = glm::length(m_position - m_target->WorldCenterPosition());
		}
		FaceTarget();
		RebuildView();
		return;
	}

	const glm::vec3 targetPosition = m_target ? m_target->WorldCenterPosition() : EvaluateProgress(m_playerProgress);
	const float desiredProgress = FindLaggedProgress(targetPosition);
	const glm::vec3 desiredPosition = EvaluateProgress(desiredProgress);
	m_desiredFollowProgress = desiredProgress;
	m_desiredFollowPosition = desiredPosition;
	m_desiredTargetDistance = glm::length(desiredPosition - targetPosition);
	m_position = glm::mix(m_position, desiredPosition, blend);
	const float actualDistance = glm::length(m_position - targetPosition);
	const bool desiredDistanceInBand = m_desiredTargetDistance >= m_minimumFollowDistance &&
		m_desiredTargetDistance <= m_maximumFollowDistance;
	if ((actualDistance < m_minimumFollowDistance || actualDistance > m_maximumFollowDistance) &&
		desiredDistanceInBand)
	{
		// Smoothing is allowed until it would cross the distance boundary. At that
		// point snap to the safe backward candidate, which is still on the authored
		// camera curve, instead of pushing radially away from the player.
		m_position = desiredPosition;
	}
	m_actualTargetDistance = glm::length(m_position - targetPosition);

	FaceTarget();

	RebuildView();
}

void PathedCamera::SetFollowSharpness(float sharpness)
{
	if (std::isfinite(sharpness))
	{
		m_followSharpness = std::max(0.0f, sharpness);
	}
}

void PathedCamera::SetMinimumFollowDistance(float distance)
{
	if (std::isfinite(distance))
	{
		m_minimumFollowDistance = std::max(0.0f, distance);
		m_maximumFollowDistance = std::max(m_maximumFollowDistance, m_minimumFollowDistance);
		m_preferredLagDistance = std::clamp(m_preferredLagDistance, m_minimumFollowDistance, m_maximumFollowDistance);
	}
}

void PathedCamera::SetMaximumFollowDistance(float distance)
{
	if (std::isfinite(distance))
	{
		m_maximumFollowDistance = std::max(m_minimumFollowDistance, distance);
		m_preferredLagDistance = std::clamp(m_preferredLagDistance, m_minimumFollowDistance, m_maximumFollowDistance);
	}
}

void PathedCamera::SetPreferredLagDistance(float distance)
{
	if (std::isfinite(distance))
		m_preferredLagDistance = std::clamp(distance, m_minimumFollowDistance, m_maximumFollowDistance);
}

glm::vec3 PathedCamera::EvaluateProgress(float progress) const
{
	if (m_path.points.empty()) return m_position;
	if (m_path.points.size() == 1) return m_path.points.front().position;
	const std::size_t segmentCount = m_path.points.size() - 1;
	const float pathPosition = glm::clamp(progress, 0.0f, 1.0f) * static_cast<float>(segmentCount);
	const std::size_t segment = std::min(static_cast<std::size_t>(pathPosition), segmentCount - 1);
	return EvaluateCameraPathSegment(m_path, segment, pathPosition - static_cast<float>(segment));
}

float PathedCamera::FindLaggedProgress(const glm::vec3& targetPosition) const
{
	if (m_path.points.size() < 2) return 0.0f;
	const int sampleCount = std::max(1, static_cast<int>(m_path.points.size() - 1) * m_pathSamplesPerSegment);
	float bestSafeProgress = 0.0f;
	float bestSafeScore = std::numeric_limits<float>::max();
	float farthestProgress = 0.0f;
	float farthestDistance = -1.0f;
	bool foundSafe = false;
	float approximatePathLength = 0.0f;
	for (std::size_t i = 1; i < m_path.points.size(); ++i)
		approximatePathLength += glm::length(m_path.points[i].position - m_path.points[i - 1].position);
	approximatePathLength = std::max(approximatePathLength, 1.0f);
	for (int sample = 0; sample <= sampleCount; ++sample)
	{
		const float progress = static_cast<float>(sample) / sampleCount;
		const float distance = glm::length(EvaluateProgress(progress) - targetPosition);
		if (distance > farthestDistance)
		{
			farthestDistance = distance;
			farthestProgress = progress;
		}
		if (distance < m_minimumFollowDistance || distance > m_maximumFollowDistance) continue;
		foundSafe = true;
		const bool behind = m_playerPathDirection >= 0.0f
			? progress <= m_playerProgress + 1e-5f
			: progress >= m_playerProgress - 1e-5f;
		const float lagError = std::abs(distance - m_preferredLagDistance);
		const float continuityDistance = std::abs(progress - m_desiredFollowProgress) * approximatePathLength;
		const float aheadPenalty = behind ? 0.0f : m_minimumFollowDistance * 0.25f;
		const float score = lagError + continuityDistance * 1.5f + aheadPenalty;
		if (score < bestSafeScore)
		{
			bestSafeScore = score;
			bestSafeProgress = progress;
		}
	}
	if (foundSafe) return bestSafeProgress;
	// No point is inside the distance band. Choose the point nearest to the band
	// rather than allowing an unbounded follow distance.
	float bestBandError = std::numeric_limits<float>::max();
	float bestBandProgress = farthestProgress;
	for (int sample = 0; sample <= sampleCount; ++sample)
	{
		const float progress = static_cast<float>(sample) / sampleCount;
		const float distance = glm::length(EvaluateProgress(progress) - targetPosition);
		const float bandError = distance < m_minimumFollowDistance
			? m_minimumFollowDistance - distance : distance - m_maximumFollowDistance;
		if (bandError < bestBandError)
		{
			bestBandError = bandError;
			bestBandProgress = progress;
		}
	}
	return bestBandProgress;
}

void PathedCamera::FaceTarget()
{
	if (!m_target || !m_target->GetMesh())
	{
		return;
	}

	const glm::vec3 direction = m_target->WorldCenterPosition() - m_position;
	if (std::isfinite(direction.x) && std::isfinite(direction.y) &&
		std::isfinite(direction.z) && glm::dot(direction, direction) > 1e-8f)
	{
		m_facing = glm::normalize(direction);
		m_up = std::abs(glm::dot(m_facing, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.999f
			? glm::vec3(0.0f, 0.0f, 1.0f)
			: glm::vec3(0.0f, 1.0f, 0.0f);
	}
}

void PathedCamera::RebuildView()
{
	m_viewMatrix = glm::lookAt(m_position, m_position + m_facing, m_up);
}
