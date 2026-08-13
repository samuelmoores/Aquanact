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
		bool valid = false;
		int avoidanceSide = 0;
		float selectedYawOffset = 0.0f;
		float selectedYaw = 0.0f;
		float targetClearanceDistance = 0.0f;
		float availableBoomDistance = 0.0f;
		float selectedPitch = 0.0f;
		bool orbitAdjusted = false;
		int collisionCount = 0;
		glm::vec3 lastNormal{ 0.0f };
		float lastPenetration = 0.0f;
		std::string lastObject;
	};

	struct CameraSideCandidate
	{
		CameraCollisionResolution resolution;
		float yawOffset = 0.0f;
	};

	const char* ColliderShapeName(PhysicsColliderShape shape)
	{
		switch (shape)
		{
		case PhysicsColliderShape::Capsule:
			return "Capsule";
		case PhysicsColliderShape::Convex:
			return "Convex";
		case PhysicsColliderShape::Box:
		default:
			return "Box";
		}
	}

	float TargetCameraClearanceDistance(
		const glm::vec3& targetPosition,
		const glm::vec3& direction,
		float cameraRadius,
		const Entity& target)
	{
		const PhysicsWorld& world = PhysicsWorld::Instance();
		const ColliderHandle handle = world.Find(target);
		if (handle == InvalidColliderHandle || handle >= world.Colliders().size())
		{
			return 0.0f;
		}

		const PhysicsCollider& collider = world.Colliders()[handle];
		const glm::vec3 furthestCorner = glm::max(
			glm::abs(collider.minBounds - targetPosition),
			glm::abs(collider.maxBounds - targetPosition));
		constexpr float collisionSkin = 0.5f;
		const float outsideDistance = glm::length(furthestCorner) +
			cameraRadius + collisionSkin;
		const glm::vec3 outsidePosition = targetPosition + direction * outsideDistance;
		const glm::vec3 inwardMovement = targetPosition - outsidePosition;

		// Sweep toward the target from beyond its complete bounds. The first hit is
		// the actual collider surface expanded by the camera sphere, not a gameplay
		// minimum-distance setting.
		const Physics::SweepCollision hit = world.SweepCameraAgainst(
			outsidePosition, cameraRadius, inwardMovement, target);
		if (!hit.hit)
		{
			return 0.0f;
		}

		const float contactDistance = outsideDistance * (1.0f - hit.time);
		return glm::min(outsideDistance, contactDistance + collisionSkin);
	}

	CameraCollisionResolution ResolveCameraBoom(
		const glm::vec3& targetPosition,
		const glm::vec3& orbitPosition,
		float colliderRadius,
		Entity* target)
	{
		CameraCollisionResolution result;
		const glm::vec3 orbitOffset = orbitPosition - targetPosition;
		const float requestedDistance = glm::length(orbitOffset);
		if (requestedDistance <= 0.0001f)
		{
			return result;
		}

		const glm::vec3 direction = orbitOffset / requestedDistance;
		const float clearanceDistance = TargetCameraClearanceDistance(
			targetPosition, direction, colliderRadius, *target);
		result.targetClearanceDistance = clearanceDistance;
		const float boomDistance = glm::max(requestedDistance, clearanceDistance);
		const glm::vec3 boomStart = targetPosition + direction * clearanceDistance;
		PhysicsWorld& world = PhysicsWorld::Instance();

		// A direction is invalid when the camera cannot fit immediately outside the
		// target collider or cannot see the target from that position. This is the
		// player-against-wall case that requires a different orbit direction.
		if (world.OverlapsCamera(boomStart, colliderRadius) ||
			!world.HasCameraLineOfSight(boomStart, targetPosition, target))
		{
			return result;
		}

		float availableDistance = boomDistance;
		const glm::vec3 outwardMovement = direction * (boomDistance - clearanceDistance);
		if (glm::length(outwardMovement) > 0.0001f)
		{
			Entity* hitObject = nullptr;
			const Physics::SweepCollision hit = world.SweepCamera(
				boomStart, colliderRadius, outwardMovement, &hitObject);
			if (hit.hit)
			{
				constexpr float collisionSkin = 0.5f;
				availableDistance = glm::max(clearanceDistance,
					clearanceDistance + glm::length(outwardMovement) * hit.time - collisionSkin);
				result.collisionCount = 1;
				result.lastNormal = hit.normal;
				result.lastObject = hitObject ? hitObject->Name() : std::string();
			}
		}
		result.availableBoomDistance = availableDistance;

		// LOS-only blockers may not participate in physical camera collision. Walk
		// inward deterministically and choose the furthest point satisfying both.
		constexpr int distanceSamples = 24;
		for (int sample = 0; sample <= distanceSamples; ++sample)
		{
			const float alpha = static_cast<float>(sample) / static_cast<float>(distanceSamples);
			const float distance = glm::mix(availableDistance, clearanceDistance, alpha);
			const glm::vec3 candidate = targetPosition + direction * distance;
			if (!world.OverlapsCamera(candidate, colliderRadius) &&
				world.HasCameraLineOfSight(candidate, targetPosition, target))
			{
				result.position = candidate;
				result.valid = true;
				return result;
			}
		}

		return result;
	}

	CameraCollisionResolution ResolveCameraAtYawOffset(
		const glm::vec3& targetPosition,
		float yaw,
		float pitch,
		float desiredDistance,
		float colliderRadius,
		Entity* target,
		int side,
		float yawOffset)
	{
		const glm::vec3 orbitPosition = BuildThirdPersonDesiredPosition(
			targetPosition, yaw + static_cast<float>(side) * yawOffset,
			pitch, desiredDistance);
		CameraCollisionResolution resolution = ResolveCameraBoom(
			targetPosition, orbitPosition, colliderRadius, target);
		if (resolution.valid)
		{
			resolution.avoidanceSide = side;
			resolution.selectedYawOffset = yawOffset;
			resolution.selectedYaw = yaw + static_cast<float>(side) * yawOffset;
			resolution.selectedPitch = pitch;
		}
		return resolution;
	}

	CameraSideCandidate FindCameraBoomOnSide(
		const glm::vec3& targetPosition,
		float yaw,
		float pitch,
		float desiredDistance,
		float colliderRadius,
		Entity* target,
		int side)
	{
		CameraSideCandidate candidate;
		constexpr float yawStep = 5.0f;
		constexpr int maximumSteps = 36;

		// Search away from the requested orbit on only one side. The first valid
		// angle is the smallest correction needed on that side of the obstacle.
		for (int step = 1; step <= maximumSteps; ++step)
		{
			const float yawOffset = static_cast<float>(step) * yawStep;
			CameraCollisionResolution resolution = ResolveCameraAtYawOffset(
				targetPosition, yaw, pitch, desiredDistance, colliderRadius,
				target, side, yawOffset);
			if (resolution.valid)
			{
				candidate.resolution = resolution;
				candidate.yawOffset = yawOffset;
				return candidate;
			}
		}

		return candidate;
	}

	CameraSideCandidate FindCameraBoomAtPitch(
		const glm::vec3& targetPosition,
		float yaw,
		float pitch,
		float desiredDistance,
		float colliderRadius,
		Entity* target)
	{
		CameraSideCandidate candidate;
		constexpr float pitchStep = 5.0f;
		constexpr int maximumSteps = 12;

		// A downward orbit can place the camera between the player capsule and the
		// floor. Search nearby pitch values because yaw-only search cannot escape.
		for (int step = 1; step <= maximumSteps; ++step)
		{
			const float offset = static_cast<float>(step) * pitchStep;
			const float pitchCandidates[2] = {
				glm::clamp(pitch + offset, -75.0f, 75.0f),
				glm::clamp(pitch - offset, -75.0f, 75.0f)
			};
			for (const float candidatePitch : pitchCandidates)
			{
				if (std::abs(candidatePitch - pitch) <= 0.001f)
				{
					continue;
				}

				const glm::vec3 orbitPosition = BuildThirdPersonDesiredPosition(
					targetPosition, yaw, candidatePitch, desiredDistance);
				CameraCollisionResolution resolution = ResolveCameraBoom(
					targetPosition, orbitPosition, colliderRadius, target);
				if (resolution.valid)
				{
					resolution.selectedYaw = yaw;
					resolution.selectedPitch = candidatePitch;
					resolution.orbitAdjusted = true;
					candidate.resolution = resolution;
					return candidate;
				}
			}
		}

		return candidate;
	}

	CameraCollisionResolution ResolveThirdPersonPosition(
		const glm::vec3& currentPosition,
		const glm::vec3& lastValidPosition,
		bool hasLastValidPosition,
		const glm::vec3& targetPosition,
		float yaw,
		float pitch,
		float orbitRadius,
		float colliderRadius,
		Entity* target,
		int preferredAvoidanceSide)
	{
		const float desiredDistance = glm::max(orbitRadius, 0.1f);

		// Use the requested orbit whenever its target-surface boom satisfies both
		// physical collision and line-of-sight constraints.
		const glm::vec3 desiredOrbit = BuildThirdPersonDesiredPosition(
			targetPosition, yaw, pitch, desiredDistance);
		CameraCollisionResolution direct = ResolveCameraBoom(
			targetPosition, desiredOrbit, colliderRadius, target);
		if (direct.valid)
		{
			direct.selectedYaw = yaw;
			direct.selectedPitch = pitch;
			return direct;
		}

		const CameraSideCandidate pitchCandidate = FindCameraBoomAtPitch(
			targetPosition, yaw, pitch, desiredDistance, colliderRadius, target);
		if (pitchCandidate.resolution.valid)
		{
			return pitchCandidate.resolution;
		}

		const CameraSideCandidate positive = FindCameraBoomOnSide(
			targetPosition, yaw, pitch, desiredDistance, colliderRadius, target, 1);
		const CameraSideCandidate negative = FindCameraBoomOnSide(
			targetPosition, yaw, pitch, desiredDistance, colliderRadius, target, -1);

		if (positive.resolution.valid && negative.resolution.valid)
		{
			const CameraSideCandidate& preferred = preferredAvoidanceSide < 0
				? negative : positive;

			// Never trade one valid side for a shorter opposite-side route. That
			// optimization caused large coordinate flips when the capsule-wall contact
			// made the two required angles alternate. Switch only if this side fails.
			if (preferredAvoidanceSide != 0)
			{
				return preferred.resolution;
			}

			if (positive.yawOffset != negative.yawOffset)
			{
				return positive.yawOffset < negative.yawOffset
					? positive.resolution : negative.resolution;
			}

			// On the first obstruction, an exact angular tie is resolved from the
			// current camera position. The chosen side then persists on later frames.
			const glm::vec3 positiveDelta = positive.resolution.position - currentPosition;
			const glm::vec3 negativeDelta = negative.resolution.position - currentPosition;
			return glm::dot(positiveDelta, positiveDelta) <=
				glm::dot(negativeDelta, negativeDelta)
				? positive.resolution : negative.resolution;
		}
		if (positive.resolution.valid)
		{
			return positive.resolution;
		}
		if (negative.resolution.valid)
		{
			return negative.resolution;
		}

		// No orbit direction worked. Keep the current coordinate only if it still
		// satisfies both constraints; otherwise recover to the last validated point.
		PhysicsWorld& world = PhysicsWorld::Instance();
		CameraCollisionResolution fallback;
		fallback.position = currentPosition;
		fallback.valid = !world.OverlapsCamera(currentPosition, colliderRadius) &&
			world.HasCameraLineOfSight(currentPosition, targetPosition, target);
		if (!fallback.valid && hasLastValidPosition &&
			!world.OverlapsCamera(lastValidPosition, colliderRadius) &&
			world.HasCameraLineOfSight(lastValidPosition, targetPosition, target))
		{
			fallback.position = lastValidPosition;
			fallback.valid = true;
		}
		return fallback;
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
	m_hasLastValidPosition = false;
	m_cameraAvoidanceSide = 0;
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
	m_hasLastValidPosition = false;
	m_cameraAvoidanceSide = 0;
	m_front = glm::normalize(facing);
	if (glm::length(m_front) <= 0.0001f)
	{
		m_front = glm::vec3(0.0f, 0.0f, 1.0f);
	}
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_view_matrix = glm::lookAt(m_position, m_position + m_front, m_up);
	m_collider->SetPosition(m_position);
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
	if (m_target != target)
	{
		m_hasLastValidPosition = false;
		m_cameraAvoidanceSide = 0;
	}
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

	const glm::vec3 orbitPosition = BuildThirdPersonDesiredPosition(targetPos, m_yaw, m_pitch, m_radius);

	if (orbitPosition == targetPos && m_radius > 0.0f)
	{
		return;
	}

	PhysicsWorld& world = PhysicsWorld::Instance();
	CameraDiagnosticsSnapshot cameraDiagnostics;
	cameraDiagnostics.lookInput = look;
	cameraDiagnostics.cameraPositionBefore = m_position;
	cameraDiagnostics.targetPosition = targetPos;
	cameraDiagnostics.orbitPosition = orbitPosition;
	cameraDiagnostics.lastValidPosition = m_lastValidPosition;
	cameraDiagnostics.dt = dt;
	cameraDiagnostics.yaw = m_yaw;
	cameraDiagnostics.pitch = m_pitch;
	cameraDiagnostics.orbitRadius = m_radius;
	cameraDiagnostics.colliderRadius = m_collider->Radius();
	cameraDiagnostics.previousAvoidanceSide = m_cameraAvoidanceSide;
	cameraDiagnostics.resolutionActiveBefore = false;
	cameraDiagnostics.currentOverlaps = world.OverlapsCamera(
		m_position, m_collider->Radius());
	cameraDiagnostics.currentHasLineOfSight = world.HasCameraLineOfSight(
		m_position, targetPos, target);
	cameraDiagnostics.hasLastValidPosition = m_hasLastValidPosition;
	const ColliderHandle targetHandle = world.Find(*target);
	if (targetHandle != InvalidColliderHandle && targetHandle < world.Colliders().size())
	{
		cameraDiagnostics.targetShape = ColliderShapeName(
			world.Colliders()[targetHandle].shape);
	}

	// Resolve a target-anchored camera boom. The target collider and world
	// colliders enforce separation without an artificial minimum distance.
	const CameraCollisionResolution resolution = ResolveThirdPersonPosition(
		m_position, m_lastValidPosition, m_hasLastValidPosition,
		targetPos, m_yaw, m_pitch, m_radius,
		m_collider->Radius(), target, m_cameraAvoidanceSide);
	cameraDiagnostics.solverValid = resolution.valid;
	cameraDiagnostics.solverPosition = resolution.position;
	cameraDiagnostics.selectedAvoidanceSide = resolution.avoidanceSide;
	cameraDiagnostics.selectedYawOffset = resolution.selectedYawOffset;
	cameraDiagnostics.targetClearanceDistance = resolution.targetClearanceDistance;
	cameraDiagnostics.availableBoomDistance = resolution.availableBoomDistance;
	cameraDiagnostics.collisionCount = resolution.collisionCount;
	cameraDiagnostics.collisionObject = resolution.lastObject;
	cameraDiagnostics.requestedMovement = resolution.position - m_position;
	if (resolution.valid)
	{
		cameraDiagnostics.solverOverlaps = world.OverlapsCamera(
			resolution.position, m_collider->Radius());
		cameraDiagnostics.solverHasLineOfSight = world.HasCameraLineOfSight(
			resolution.position, targetPos, target);
	}

	if (resolution.valid && resolution.avoidanceSide != 0)
	{
		m_cameraAvoidanceSide = resolution.avoidanceSide;
	}
	// Commit the solver result immediately. There is no automatic correction
	// speed or interpolation: the camera changes only from this frame's input,
	// with collision resolution acting as an instantaneous positional constraint.
	glm::vec3 resolvedPosition = resolution.valid ? resolution.position : m_position;
	if (resolution.valid && resolution.avoidanceSide == 0)
	{
		m_cameraAvoidanceSide = 0;
	}

	if (resolution.valid &&
		!world.OverlapsCamera(resolvedPosition, m_collider->Radius()) &&
		world.HasCameraLineOfSight(resolvedPosition, targetPos, target))
	{
		m_lastValidPosition = resolvedPosition;
		m_hasLastValidPosition = true;
	}
	cameraDiagnostics.committedPosition = resolvedPosition;
	cameraDiagnostics.appliedMovement = resolvedPosition - m_position;
	cameraDiagnostics.resolutionActiveAfter = false;
	cameraDiagnostics.committedOverlaps = world.OverlapsCamera(
		resolvedPosition, m_collider->Radius());
	cameraDiagnostics.committedHasLineOfSight = world.HasCameraLineOfSight(
		resolvedPosition, targetPos, target);
	cameraDiagnostics.lastValidPosition = m_lastValidPosition;
	cameraDiagnostics.hasLastValidPosition = m_hasLastValidPosition;

	// Commit the resolved position and rebuild the view direction.
	m_position = resolvedPosition;
	m_collider->SetPosition(m_position);
	m_front = glm::normalize(targetPos - m_position);

	RebuildView();

	// Publish collision information for the physics diagnostics UI.
	Root::Current().Debugger().SetPhysicsDiagnostics(
		m_position, orbitPosition, resolvedPosition, m_collider->Radius(),
		resolution.collisionCount, resolution.lastNormal, resolution.lastPenetration,
		resolution.lastObject);
	Root::Current().Debugger().SetCameraDiagnostics(cameraDiagnostics);
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


