#include "Game/PlayerController.h"

#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/RenderManager.h"

#include <algorithm>

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

	glm::vec3 BuildWorldMovement(const glm::vec2& move2D)
	{
		const glm::vec3 forward = CameraForwardVector();
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
		float yawDelta = targetYaw - currentYaw;
		while (yawDelta > glm::pi<float>())
		{
			yawDelta -= glm::two_pi<float>();
		}
		while (yawDelta < -glm::pi<float>())
		{
			yawDelta += glm::two_pi<float>();
		}
		const float maxStep = std::max(0.0f, turnSpeed) * dt;
		const float nextYaw = currentYaw + std::clamp(yawDelta, -maxStep, maxStep);
		owner.SetRotation(glm::vec3(owner.Rotation().x, nextYaw, owner.Rotation().z));
	}

}

float PlayerController::WrapAngle(float angle)
{
	while (angle > glm::pi<float>())
	{
		angle -= glm::two_pi<float>();
	}
	while (angle < -glm::pi<float>())
	{
		angle += glm::two_pi<float>();
	}
	return angle;
}

float PlayerController::ShortestAngleDelta(float from, float to)
{
	return WrapAngle(to - from);
}

float PlayerController::GravityScale() const
{
	// Increase gravity during both halves of the jump so the full arc completes
	// faster. The stronger downward scale still makes the descent decisive.
	//return m_velocity.y > 0.0f ? 1.875f : 5.0f;
	return 3.0f;
}

void PlayerController::TryJump(const InputManager& input)
{
	// Step 1: accept only a new Jump press while grounded. This prevents held
	// input from creating repeated jumps and leaves landing to the physics code.
	if (!input.WasPressed("Jump") || !m_grounded)
	{
		return;
	}

	// Step 2: wait until the state machine has left Falling. Physics can report
	// ground contact one frame before Falling -> Idle/Run is evaluated. Launching
	// during that gap immediately re-enters the falling animation and interrupts
	// the landing transition.
	if (m_entityState && (m_entityState->CurrentState() == "Falling"
		|| m_entityState->CurrentState() == "Landing"))
	{
		return;
	}

	// Step 3: leave the grounded state before applying vertical launch velocity.
	// MoveWithPhysics() clears vertical velocity while grounded, so this order is
	// required for the jump impulse to survive the movement step.
	//m_velocity.y = m_jumpSpeed;

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
	Controller::Move(owner, move2D, dt);
}


void PlayerController::Update(Entity& owner, float dt)
{
	const InputManager& input = m_inputActions ? *m_inputActions : Root::Current().InputActions();
	const glm::vec2 move2D = input.VectorValue("Move");
	m_wantsToMove = glm::length(move2D) > 0.0001f;

	if (m_entityState->CurrentStateBlocksMovement())
	{
		// Keep the movement bindable synchronized while movement is blocked. If
		// this is skipped, IsMoving can remain true from the previous Run frame and
		// incorrectly select Punch -> Run when the attack animation completes.
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
