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
		glm::vec3 forward = Root::Current().Render().GetGameCamera().GetFacing();
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

	void FaceMovementDirection(Entity& owner, const glm::vec3& direction, float turnSpeed, float dt, bool grounded)
	{
		// Only rotate when grounded so airborne movement does not fight physics.
		if (!grounded || turnSpeed <= 0.0f)
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

void PlayerController::TryJump(const InputManager& input, const glm::vec2& move2D)
{
	// Step 1: accept only a new Jump press while grounded. This prevents held
	// input from creating repeated jumps and leaves landing to the physics code.
	if (!input.WasPressed("Jump") || !m_isGrounded)
	{
		return;
	}

	// Step 2: leave the grounded state before applying vertical launch velocity.
	// MoveWithPhysics() clears vertical velocity while grounded, so this order is
	// required for the jump impulse to survive the movement step.
	m_grounded = false;
	m_isGrounded = false;
	m_groundedLossTimer = 0.0f;
	m_velocity.y = m_jumpSpeed;

	// Step 3: capture horizontal launch velocity in world space. The cached value
	// is not rebuilt from input while airborne, which makes the jump projectile-like.
	const glm::vec3 launchMovement = BuildWorldMovement(move2D);
	m_jumpHorizontalVelocity = glm::length(launchMovement) > 0.0001f
		? glm::normalize(launchMovement) * m_moveSpeed
		: glm::vec3(0.0f);
	m_jumpTrajectoryLocked = true;
}

void PlayerController::startUp(Entity& owner)
{
	Controller::startUp(owner);
	m_wantsToMove = false;
	m_jumpHorizontalVelocity = glm::vec3(0.0f);
	m_jumpTrajectoryLocked = false;
	m_inputActions = &Root::Current().InputActions();
}

void PlayerController::FirstFrame(Entity& owner)
{
	m_entityState = owner.GetComponent<EntityStateMachine>();
}

void PlayerController::Move(Entity& owner, const glm::vec2& move2D, float dt)
{
	// Keep a 3D version of the input around for diagnostics and animation.
	const glm::vec3 moveInput(move2D.x, 0.0f, move2D.y);
	MoveWithWorldDirection(owner, BuildWorldMovement(move2D), moveInput, dt);
}

void PlayerController::MoveWithWorldDirection(
	Entity& owner,
	const glm::vec3& movement,
	const glm::vec3& diagnosticInput,
	float dt)
{
	SetDiagnosticInput(diagnosticInput);
	SetMovementDirection(movement);

	// Exact zero input means no movement this frame, but we still skip the
	// normalization step to avoid dividing by zero.
	if (movement == glm::vec3(0.0f))
	{
		m_isMoving = false;
		const glm::vec3 appliedDelta = MoveWithPhysics(owner, glm::vec3(0.0f), dt);
		Root::Current().Debugger().SetGameplayDiagnostics(owner.Name(), diagnosticInput, m_moveSpeed, dt, appliedDelta, owner.Position());
		return;
	}

	const glm::vec3 normalizedMovement = glm::normalize(movement);
	m_isMoving = true;

	// When grounded, turn toward the travel direction so the character faces the
	// way it is moving instead of sliding sideways.
	if (m_grounded)
	{
		FaceMovementDirection(owner, normalizedMovement, m_turnSpeed, dt, m_grounded);
	}

	// Apply the final horizontal velocity through the physics path so collision
	// and diagnostics stay consistent with the rest of the controller.
	const glm::vec3 desiredHorizontalVelocity = normalizedMovement * m_moveSpeed;
	const glm::vec3 appliedDelta = MoveWithPhysics(owner, desiredHorizontalVelocity, dt);
	Root::Current().Debugger().SetGameplayDiagnostics(owner.Name(), diagnosticInput, m_moveSpeed, dt, appliedDelta, owner.Position());
}

void PlayerController::Update(Entity& owner, float dt)
{
	const InputManager& input = m_inputActions ? *m_inputActions : Root::Current().InputActions();
	const glm::vec2 move2D = input.VectorValue("Move");
	m_wantsToMove = glm::length(move2D) > 0.0001f;

	// Landing ends the projectile phase. The next grounded frame can once again
	// use the player's live movement input.
	if (m_isGrounded)
	{
		m_jumpTrajectoryLocked = false;
	}

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
	TryJump(input, move2D);
	if (m_jumpTrajectoryLocked && !m_isGrounded)
	{
		// Step 4: use the cached world-space launch velocity while airborne. The
		// movement helper still applies gravity and collision resolution normally.
		const glm::vec3 diagnosticInput(
			m_jumpHorizontalVelocity.x,
			0.0f,
			m_jumpHorizontalVelocity.z);
		MoveWithWorldDirection(owner, m_jumpHorizontalVelocity, diagnosticInput, dt);
	}
	else
	{
		Move(owner, move2D, dt);
	}
}
