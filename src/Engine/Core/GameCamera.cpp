#include "Engine/Core/GameCamera.h"

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
	RebuildView();
}

void PathedCamera::SetPath(const CameraPathData& path)
{
	m_path = path;
	if (!m_path.points.empty())
	{
		m_position = m_path.points.front().position;
		FaceTarget();
		RebuildView();
	}
}

void PathedCamera::SetTarget(Entity* target)
{
	m_target = target;
	FaceTarget();
	RebuildView();
}

void PathedCamera::SetPlayerProgress(float progress)
{
	if (std::isfinite(progress))
	{
		m_playerProgress = std::max(0.0f, progress);
	}
}

float PathedCamera::ClosestPathDistance(const glm::vec3& worldPosition) const
{
	if (m_path.points.empty()) return 0.0f;
	if (m_path.points.size() == 1) return 0.0f;

	float bestDistanceSquared = std::numeric_limits<float>::max();
	float bestPathDistance = m_path.points.front().playerProgress;
	float accumulatedDistance = 0.0f;
	for (std::size_t i = 1; i < m_path.points.size(); ++i)
	{
		const glm::vec3 start = m_path.points[i - 1].position;
		const glm::vec3 segment = m_path.points[i].position - start;
		const float segmentLengthSquared = glm::dot(segment, segment);
		const float t = segmentLengthSquared > 1e-8f
			? glm::clamp(glm::dot(worldPosition - start, segment) / segmentLengthSquared, 0.0f, 1.0f)
			: 0.0f;
		const glm::vec3 closest = start + segment * t;
		const float distanceSquared = glm::dot(worldPosition - closest, worldPosition - closest);
		if (distanceSquared < bestDistanceSquared)
		{
			bestDistanceSquared = distanceSquared;
			bestPathDistance = accumulatedDistance + glm::length(segment) * t;
		}
		accumulatedDistance += glm::length(segment);
	}
	return bestPathDistance;
}

void PathedCamera::Update(float deltaTime)
{
	const float blend = 1.0f - std::exp(-std::max(0.0f, m_followSharpness) * std::max(0.0f, deltaTime));
	if (m_path.points.empty())
	{
		return;
	}

	if (m_path.points.size() == 1 ||
		m_playerProgress <= 0.0f)
	{
		m_position = glm::mix(m_position, m_path.points.front().position, blend);
		FaceTarget();
		RebuildView();
		return;
	}

	float segmentStartDistance = 0.0f;
	for (std::size_t i = 1; i < m_path.points.size(); ++i)
	{
		const CameraPathPoint& end = m_path.points[i];
		const CameraPathPoint& start = m_path.points[i - 1];
		const float segmentLength = glm::length(end.position - start.position);
		if (m_playerProgress <= segmentStartDistance + segmentLength)
		{
			const float range = segmentLength;
			const float t = range > 0.0f
				? glm::clamp((m_playerProgress - segmentStartDistance) / range, 0.0f, 1.0f)
				: 1.0f;
			const glm::vec3 desiredPosition = glm::mix(start.position, end.position, t);
			m_position = glm::mix(m_position, desiredPosition, blend);
			FaceTarget();
			RebuildView();
			return;
		}
		segmentStartDistance += segmentLength;
	}

	// Hold the final camera point after the player passes the path end.
	m_position = glm::mix(m_position, m_path.points.back().position, blend);
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

void PathedCamera::FaceTarget()
{
	if (!m_target)
	{
		return;
	}

	const glm::vec3 direction = m_target->WorldCenterPosition() - m_position;
	if (std::isfinite(direction.x) && std::isfinite(direction.y) &&
		std::isfinite(direction.z) && glm::dot(direction, direction) > 1e-8f)
	{
		m_facing = glm::normalize(direction);
	}
}

void PathedCamera::RebuildView()
{
	m_viewMatrix = glm::lookAt(m_position, m_position + m_facing, {0.0f, 1.0f, 0.0f});
}
