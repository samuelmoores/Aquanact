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
#include "Engine/Core/PhysicsWorld.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <cmath>

namespace
{
	glm::vec3 BuildThirdPersonDesiredPosition(const glm::vec3& targetPosition,
		float yaw, float pitch, float radius)
	{
		const float yawRad = glm::radians(yaw);
		const float pitchRad = glm::radians(pitch);
		glm::vec3 offset(
			std::sin(yawRad) * std::cos(pitchRad),
			std::sin(pitchRad),
			std::cos(yawRad) * std::cos(pitchRad));

		if (!std::isfinite(offset.x) || !std::isfinite(offset.y) ||
			!std::isfinite(offset.z) || glm::length(offset) <= 0.0001f)
		{
			return targetPosition;
		}

		return targetPosition - glm::normalize(offset) * radius;
	}

	struct CameraCollisionResolution
	{
		glm::vec3 position{ 0.0f };
		int collisionCount = 0;
		glm::vec3 lastNormal{ 0.0f };
		float lastPenetration = 0.0f;
		std::string lastObject;
	};

	glm::vec3 RecoverCameraStartPosition(
		const glm::vec3& startPosition,
		const glm::vec3& lastSafePosition,
		bool hasSafePosition,
		float colliderRadius,
		const Entity* target)
	{
		const bool startBlocked = PhysicsWorld::Instance().OverlapsCamera(
			startPosition, colliderRadius, target);
		const bool safePositionAvailable = hasSafePosition &&
			!PhysicsWorld::Instance().OverlapsCamera(lastSafePosition, colliderRadius, target);

		// If the current camera position is invalid, use the last known valid
		// position before attempting to follow the target again.
		return startBlocked && safePositionAvailable ? lastSafePosition : startPosition;
	}

	glm::vec3 ResolveCameraSlide(
		const glm::vec3& position,
		const glm::vec3& movement,
		float colliderRadius,
		const Entity* target,
		CameraCollisionResolution& result)
	{
		Entity* hitObject = nullptr;
		const Physics::SweepCollision hit = PhysicsWorld::Instance().SweepCamera(
			position, colliderRadius, movement, target, &hitObject);
		if (!hit.hit)
		{
			result.position = position + movement;
			return glm::vec3(0.0f);
		}

		++result.collisionCount;
		result.lastNormal = hit.normal;
		result.lastObject = hitObject ? hitObject->Name() : std::string();

		constexpr float collisionSkin = 0.05f;
		const float movementLength = glm::length(movement);
		const float safeTime = glm::max(0.0f,
			hit.time - collisionSkin / movementLength);
		result.position = position + movement * safeTime;

		// Remove movement into the surface and return only the tangent movement
		// for the next slide iteration.
		glm::vec3 slideMovement = movement * (1.0f - hit.time);
		const float intoSurface = glm::dot(slideMovement, hit.normal);
		if (intoSurface < 0.0f)
		{
			slideMovement -= hit.normal * intoSurface;
		}
		return slideMovement;
	}

	CameraCollisionResolution ResolveThirdPersonPosition(
		const glm::vec3& startPosition,
		const glm::vec3& desiredPosition,
		const glm::vec3& lastSafePosition,
		bool hasSafePosition,
		float colliderRadius,
		const Entity* target)
	{
		CameraCollisionResolution result;
		// First recover from an invalid starting position if a previous safe
		// position is available.
		result.position = RecoverCameraStartPosition(
			startPosition, lastSafePosition, hasSafePosition, colliderRadius, target);

		// Then follow the desired orbit position through at most a few slide
		// iterations, stopping when no movement remains.
		glm::vec3 remainingMovement = desiredPosition - result.position;
		constexpr int maxSlideIterations = 3;
		for (int iteration = 0;
			iteration < maxSlideIterations && glm::length(remainingMovement) > 0.0001f;
			++iteration)
		{
			remainingMovement = ResolveCameraSlide(
				result.position, remainingMovement, colliderRadius, target, result);
		}

		// Reject a result that still leaves the camera inside geometry. The caller
		// will retain the original position in this failure case.
		if (PhysicsWorld::Instance().OverlapsCamera(result.position, colliderRadius, target))
		{
			result.position = startPosition;
		}
		return result;
	}
}

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

void GameCamera::CaptureEditorState()
{
	m_editorPosition = m_position;
	m_editorFacing = m_front;
	m_editorRadius = m_radius;
	m_editorYaw = m_yaw;
	m_editorPitch = m_pitch;
	m_editorColliderRadius = ColliderRadius();
	m_hasEditorState = true;
}

void GameCamera::RestoreEditorState()
{
	if (!m_hasEditorState)
	{
		return;
	}

	SetPose(m_editorPosition, m_editorFacing);
	SetRadius(m_editorRadius);
	SetOrbitAngles(m_editorYaw, m_editorPitch);
	SetColliderRadius(m_editorColliderRadius);
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
	// Read orbit input and update the camera's yaw and pitch.
	(void)input;
	const glm::vec2 look = Root::Current().InputActions().VectorValue("Look");

	if (glm::length(look) > 0.0f)
	{
		const float stepScale = glm::max(dt, 0.0001f) * 120.0f;

		m_yaw -= look.x * m_lookSensitivity * stepScale;

		m_pitch = glm::clamp(m_pitch + look.y * m_lookSensitivity * stepScale, -75.0f, 75.0f);
	}

	// Validate the follow target before calculating an orbit position.
	Entity* target = m_target;
	if (!target)
	{
		return;
	}

	// Build the desired orbit position from the interpolated target pose.
	// Follow the same interpolated target pose used by rendering. Following the
	// raw physics pose here makes the camera jump whenever the fixed-step
	// accumulator performs zero or multiple controller updates in a frame.
	const glm::vec3 targetPos = target->WorldCenterPosition();

	if (!std::isfinite(targetPos.x) || !std::isfinite(targetPos.y) || !std::isfinite(targetPos.z))
	{
		return;
	}

	const glm::vec3 desiredPosition = BuildThirdPersonDesiredPosition(targetPos, m_yaw, m_pitch, m_radius);

	if (desiredPosition == targetPos && m_radius > 0.0f)
	{
		return;
	}

	// 4. Resolve the desired orbit position against the physics world. This
	// includes safe-position recovery and sliding around obstructions.
	const CameraCollisionResolution resolution = ResolveThirdPersonPosition(
		m_position, desiredPosition, m_lastSafePosition, m_hasSafePosition,
		m_collider->Radius(), target);

	const glm::vec3 resolvedPosition = resolution.position;

	if (resolvedPosition != m_position)
	{
		m_lastSafePosition = resolvedPosition;
		m_hasSafePosition = true;
	}

	// Commit the resolved position and rebuild the view direction.
	m_position = resolvedPosition;
	m_collider->SetPosition(m_position);
	m_front = glm::normalize(targetPos - m_position);

	RebuildView();

	// Publish collision information for the physics diagnostics UI.
	Root::Current().Debugger().SetPhysicsDiagnostics(
		m_position, desiredPosition, resolvedPosition, m_collider->Radius(),
		resolution.collisionCount, resolution.lastNormal, resolution.lastPenetration,
		resolution.lastObject);
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


