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
	RebuildView();
}

void PathedCamera::SetPath(const CameraPathData& path)
{
	m_path = IsValidCameraPath(path) ? path : CameraPathData{};
	if (!m_path.points.empty())
	{
		m_position = m_path.points.front().position;
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
		m_playerProgress = glm::clamp(progress, 0.0f, 1.0f);
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
		return;
	}

	if (m_path.points.size() == 1 || m_playerProgress <= 0.0f)
	{
		m_position = glm::mix(m_position, m_path.points.front().position, blend);
		FaceTarget();
		RebuildView();
		return;
	}

	const std::size_t segmentCount = m_path.points.size() - 1;
	const float pathPosition = glm::clamp(m_playerProgress, 0.0f, 1.0f) * static_cast<float>(segmentCount);
	const std::size_t segment = std::min(static_cast<std::size_t>(pathPosition), segmentCount - 1);
	const float t = pathPosition - static_cast<float>(segment);
	const glm::vec3 desiredPosition = EvaluateCameraPathSegment(m_path, segment, t);
	m_position = glm::mix(m_position, desiredPosition, blend);
	FaceTarget();
	RebuildView();
	return;

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
	if (!m_target || !m_target->GetMesh())
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
