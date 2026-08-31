#include "Game/PlayerController.h"

#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
	// Build a normalized camera-relative movement basis. If the camera is in a
	// degenerate orientation, fall back to world axes so movement still works.
	glm::vec3 CameraForwardVector()
	{
		glm::vec3 forward = Root::Current().Render().GetPathedCamera().GetFacing();
		forward.y = 0.0f;
		if (glm::length(forward) <= 0.0001f)
		{
			return glm::vec3(0.0f, 0.0f, 1.0f);
		}
		return glm::normalize(forward);
	}

	glm::vec3 CameraRightVector(const glm::vec3& forward)
	{
		glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
		if (glm::length(right) <= 0.0001f)
		{
			return glm::vec3(1.0f, 0.0f, 0.0f);
		}
		return glm::normalize(right);
	}

	glm::vec3 BuildWorldMovement(const glm::vec2& move2D, const glm::vec3& forward)
	{
		const glm::vec3 right = CameraRightVector(forward);

		glm::vec3 movement = forward * move2D.y + right * move2D.x;
		movement.y = 0.0f;
		return movement;
	}

	void FaceMovementDirection(Entity& owner, const glm::vec3& direction, float turnSpeed, float dt)
	{
		// Rotation follows live input both on the ground and in the air.
		if (turnSpeed <= 0.0f)
		{
			return;
		}

		const float targetYaw = std::atan2(direction.x, direction.z);
		const float currentYaw = owner.Rotation().y;
		const float yawDelta = MathUtils::ShortestAngleDelta(currentYaw, targetYaw);
		const float maxStep = std::max(0.0f, turnSpeed) * dt;
		const float nextYaw = currentYaw + std::clamp(yawDelta, -maxStep, maxStep);
		owner.SetRotation(glm::vec3(owner.Rotation().x, nextYaw, owner.Rotation().z));
	}

}

void PlayerController::PreserveCameraDirection(const glm::vec3& direction)
{
	glm::vec3 horizontalDirection = direction;
	horizontalDirection.y = 0.0f;
	if (std::isfinite(horizontalDirection.x) && std::isfinite(horizontalDirection.z) &&
		glm::dot(horizontalDirection, horizontalDirection) > 1e-8f)
	{
		m_cameraDirectionOverride = glm::normalize(horizontalDirection);
		m_hasCameraDirectionOverride = true;
	}
}

float PlayerController::GravityScale() const
{
	// Increase gravity during both halves of the jump so the full arc completes
	// faster. The stronger downward scale still makes the descent decisive.
	return m_velocity.y > 0.0f ? 1.875f : 5.0f;
}

void PlayerController::TryJump(const InputManager& input)
{
	// Step 1: accept only a new Jump press while grounded. This prevents held
	// input from creating repeated jumps and leaves landing to the physics code.
	if (!input.WasPressed("Jump"))
	{
		return;
	}

	// Leave the grounded state before applying vertical launch velocity. The
	// physics controller owns the authoritative grounded result; animation state
	// names must not veto a valid jump impulse.
	// MoveWithPhysics() clears vertical velocity while grounded, so this order is
	// required for the jump impulse to survive the movement step.
	m_grounded = false;
	m_velocity.y = m_jumpSpeed;

}

void PlayerController::startUp(Entity& owner)
{
	Controller::startUp(owner);
	m_wantsToMove = false;
	m_inputActions = &Root::Current().InputActions();
}

void PlayerController::OnTriggerEnter(Entity& triggerOwner)
{
	// Trigger behavior is intentionally empty until gameplay-specific effects
	// are assigned. The override provides a concrete hook for player triggers.
	std::cout << "Player Controller entered trigger sphere owned by: " << triggerOwner.Name() << std::endl;
}

void PlayerController::FirstFrame(Entity& owner)
{
	m_entityState = owner.GetComponent<EntityStateMachine>();
}

void PlayerController::Move(Entity& owner, const glm::vec2& move2D, float dt)
{
	const glm::vec3 cameraForward = m_hasCameraDirectionOverride
		? m_cameraDirectionOverride
		: CameraForwardVector();
	const glm::vec3 worldMovement = BuildWorldMovement(move2D, cameraForward);
	const bool hasMovement = glm::length(worldMovement) > 0.0001f;

	if (hasMovement)
	{
		FaceMovementDirection(owner, worldMovement, m_turnSpeed, dt);
	}

	// Controller::Move expects a horizontal X/Z vector. Convert the
	// camera-relative world direction back into that shared representation.
	Controller::Move(owner, glm::vec2(worldMovement.x, worldMovement.z), dt);
}


void PlayerController::Update(Entity& owner, float dt)
{
	const InputManager& input = m_inputActions ? *m_inputActions : Root::Current().InputActions();
	const glm::vec2 move2D = input.VectorValue("Move");
	m_wantsToMove = glm::length(move2D) > 0.0001f;
	if (!m_wantsToMove)
	{
		m_hasCameraDirectionOverride = false;
	}

	if (m_entityState && m_entityState->CurrentStateBlocksMovement())
	{
		// Blocking locomotion must not block gravity, ground probing, or landing.
		// Otherwise Jump/Landing/Punch can freeze the controller's last grounded
		// value and leave the animation state machine stuck in an airborne state.
		Controller::Update(owner, dt);
		Move(owner, glm::vec2(0.0f), dt);
		return;
	}

	// A jump can only be requested after state-based movement blocking has been
	// handled. This prevents attacks or other blocking states from launching.
	TryJump(input);
	Controller::Update(owner, dt);
	// Use live input for horizontal movement and facing throughout the jump.
	Move(owner, move2D, dt);
}
