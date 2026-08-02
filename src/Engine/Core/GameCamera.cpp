#include "Engine/Core/GameCamera.h"

#include "Engine/Core/CameraCollider.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/EngineCamera.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Debug.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <cmath>

GameCamera::GameCamera()
	: m_collider(std::make_unique<CameraCollider>())
{
}

GameCamera::~GameCamera() = default;

void GameCamera::startUp()
{
	m_window = Root::Current().WindowRef().GLFW();

	int width = 1;
	int height = 1;
	glfwGetWindowSize(m_window, &width, &height);
	if (height <= 0)
	{
		height = 1;
	}

	m_projection_matrix = glm::perspective(glm::radians(m_fieldOfView), static_cast<float>(width) / static_cast<float>(height), m_nearPlane, m_farPlane);
	m_position = glm::vec3(0.0f, 0.0f, -10.0f);
	m_front = glm::vec3(0.0f, 0.0f, 1.0f);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_view_matrix = glm::lookAt(m_position, glm::vec3(0.0f), m_up);
	m_collider->SetPosition(m_position);
	m_lastSafePosition = m_position;
	m_hasSafePosition = true;
}

void GameCamera::shutDown()
{
}

glm::mat4 GameCamera::GetProjectionMatrix() const
{
	int width = 1;
	int height = 1;
	glfwGetWindowSize(m_window, &width, &height);
	if (height <= 0)
	{
		height = 1;
	}

	return glm::perspective(glm::radians(m_fieldOfView), static_cast<float>(width) / static_cast<float>(height), m_nearPlane, m_farPlane);
}

glm::mat4 GameCamera::GetViewMatrix() const
{
	return m_view_matrix;
}

glm::vec3 GameCamera::GetPosition() const
{
	return m_position;
}

glm::vec3 GameCamera::GetFacing() const
{
	return m_front;
}

void GameCamera::CopyFrom(const EngineCamera& camera)
{
	SetPose(camera.GetPosition(), camera.GetFacing());
	m_projection_matrix = camera.GetProjectionMatrix();
}

void GameCamera::SetPose(const glm::vec3& position, const glm::vec3& facing)
{
	m_position = position;
	m_front = glm::normalize(facing);
	if (glm::length(m_front) <= 0.0001f)
	{
		m_front = glm::vec3(0.0f, 0.0f, 1.0f);
	}
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_view_matrix = glm::lookAt(m_position, m_position + m_front, m_up);
	m_collider->SetPosition(m_position);
	m_lastSafePosition = m_position;
	m_hasSafePosition = true;
	m_yaw = glm::degrees(std::atan2(m_front.x, m_front.z));
	m_pitch = glm::degrees(std::asin(glm::clamp(m_front.y, -1.0f, 1.0f)));
	if (m_target)
	{
		m_radius = glm::length(m_target->WorldCenterPosition() - m_position);
	}
}

void GameCamera::SetTarget(Entity* target)
{
	m_target = target;
	if (!m_target)
	{
		m_targetId = 0;
		m_targetName.clear();
		return;
	}

	m_targetId = m_target->Id();
	m_targetName = m_target->Name();

	if (m_target)
	{
		const glm::vec3 targetCenter = m_target->WorldCenterPosition();
		const float distance = glm::length(m_position - targetCenter);
		m_radius = distance > 0.1f ? distance : m_radius;
	}
}

void GameCamera::SetRadius(float radius)
{
	m_radius = glm::max(radius, 0.1f);
}

void GameCamera::SetOrbitAngles(float yaw, float pitch)
{
	m_yaw = yaw;
	m_pitch = glm::clamp(pitch, -75.0f, 75.0f);
}

void GameCamera::RebuildView()
{
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_view_matrix = glm::lookAt(m_position, m_position + m_front, m_up);
}

void GameCamera::UpdateThirdPerson(const Input& input, float dt)
{
	(void)input;
	const glm::vec2 look = Root::Current().InputActions().VectorValue("Look");
	if (glm::length(look) > 0.0f)
	{
		const float stepScale = glm::max(dt, 0.0001f) * 120.0f;
		m_yaw -= look.x * m_lookSensitivity * stepScale;
		m_pitch = glm::clamp(m_pitch + look.y * m_lookSensitivity * stepScale, -75.0f, 75.0f);
	}

	Entity* target = m_target;
	if (!target)
	{
		return;
	}

	const glm::vec3 targetPos = target->WorldCenterPosition();
	if (!std::isfinite(targetPos.x) || !std::isfinite(targetPos.y) || !std::isfinite(targetPos.z))
	{
		return;
	}
	const float yawRad = glm::radians(m_yaw);
	const float pitchRad = glm::radians(m_pitch);
	glm::vec3 offset;
	offset.x = std::sin(yawRad) * std::cos(pitchRad);
	offset.y = std::sin(pitchRad);
	offset.z = std::cos(yawRad) * std::cos(pitchRad);
	if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z) || glm::length(offset) <= 0.0001f)
	{
		return;
	}
	offset = glm::normalize(offset) * m_radius;
	const glm::vec3 desiredPosition = targetPos - offset;
	glm::vec3 resolvedPosition = m_position;
	int collisionCount = 0;
	glm::vec3 lastCollisionNormal(0.0f);
	float lastPenetration = 0.0f;
	std::string lastCollisionObject;
	const Scene* activeLevel = Root::Current().Levels().ActiveLevel();
	if (activeLevel)
	{
		const auto positionBlocked = [&](const glm::vec3& position)
		{
			m_collider->SetPosition(position);
			for (const auto& object : activeLevel->Objects())
			{
				if (!object || object.get() == target || object->IgnoreCameraCollision() || !object->GetMesh())
				{
					continue;
				}

				glm::vec3 boxMin;
				glm::vec3 boxMax;
				if (object->WorldAABB(boxMin, boxMax) && m_collider->OverlapsAABB(boxMin, boxMax))
				{
					return true;
				}
			}
			return false;
		};

		const glm::vec3 frameStartPosition = m_position;
		if (positionBlocked(m_position) && m_hasSafePosition && !positionBlocked(m_lastSafePosition))
		{
			resolvedPosition = m_lastSafePosition;
		}

		glm::vec3 remainingMovement = desiredPosition - resolvedPosition;
		constexpr int maxSlideIterations = 3;
		constexpr float collisionSkin = 0.05f;
		for (int iteration = 0; iteration < maxSlideIterations && glm::length(remainingMovement) > 0.0001f; ++iteration)
		{
			m_collider->SetPosition(resolvedPosition);
			Physics::SweepCollision earliestHit;
			Entity* hitObject = nullptr;
			for (const auto& object : activeLevel->Objects())
			{
				if (!object || object.get() == target || object->IgnoreCameraCollision() || !object->GetMesh())
				{
					continue;
				}

				glm::vec3 boxMin;
				glm::vec3 boxMax;
				if (!object->WorldAABB(boxMin, boxMax))
				{
					continue;
				}

				const Physics::SweepCollision hit = m_collider->SweepAgainstAABB(remainingMovement, boxMin, boxMax);
				if (hit.hit && hit.time < earliestHit.time)
				{
					earliestHit = hit;
					hitObject = object.get();
				}
			}

			if (!earliestHit.hit)
			{
				resolvedPosition += remainingMovement;
				remainingMovement = glm::vec3(0.0f);
				break;
			}

			++collisionCount;
			lastCollisionNormal = earliestHit.normal;
			lastPenetration = 0.0f;
			lastCollisionObject = hitObject ? hitObject->Name() : std::string();
			const float movementLength = glm::length(remainingMovement);
			const float safeTime = glm::max(0.0f, earliestHit.time - collisionSkin / movementLength);
			resolvedPosition += remainingMovement * safeTime;

			glm::vec3 slideMovement = remainingMovement * (1.0f - earliestHit.time);
			const float intoSurface = glm::dot(slideMovement, earliestHit.normal);
			if (intoSurface < 0.0f)
			{
				slideMovement -= earliestHit.normal * intoSurface;
			}
			remainingMovement = slideMovement;
		}

		if (positionBlocked(resolvedPosition))
		{
			resolvedPosition = frameStartPosition;
		}
		else
		{
			m_lastSafePosition = resolvedPosition;
			m_hasSafePosition = true;
		}
	}
	else
	{
		resolvedPosition = desiredPosition;
		m_lastSafePosition = resolvedPosition;
		m_hasSafePosition = true;
	}

	m_position = resolvedPosition;
	m_collider->SetPosition(m_position);
	m_front = glm::normalize(targetPos - m_position);
	RebuildView();
	Root::Current().Debugger().SetPhysicsDiagnostics(
		m_position, desiredPosition, resolvedPosition, m_collider->Radius(),
		collisionCount, lastCollisionNormal, lastPenetration, lastCollisionObject);
}

CameraCollider& GameCamera::Collider()
{
	return *m_collider;
}

float GameCamera::ColliderRadius() const
{
	return m_collider->Radius();
}

void GameCamera::SetColliderRadius(float radius)
{
	m_collider->SetRadius(radius);
}

const CameraCollider& GameCamera::Collider() const
{
	return *m_collider;
}


