#pragma once

#include "Engine/Core/Component.h"
#include "glm/glm.hpp"

class Entity;

class Controller : public Component {
public:
	Controller() = default;

	const char* Name() const override { return "Controller"; }
	int ExecutionOrder() const override { return -100; }
	void startUp(Entity&) override;

	// Movement tuning. These values control how the controller behaves.
	float MoveSpeed() const { return m_moveSpeed; }
	void SetMoveSpeed(float moveSpeed) { m_moveSpeed = moveSpeed; }

	// Movement intent. These expose what the controller is trying to do this frame.
	const glm::vec3& MovementDirection() const { return m_movementDirection; }
	bool IsMoving() const { return m_isMoving; }
	bool IsGrounded() const { return m_isGrounded; }
	float GroundSurfaceAngle() const;

	// Bindable names used by entity state transitions and editor condition pickers.
	// These stay stable so saved state machine graphs continue to resolve.
	#define CONTROLLER_BINDABLES(VALUE, FUNCTION) \
		FUNCTION(IsMoving) \
		FUNCTION(IsGrounded) \
		VALUE(m_moveSpeed)
	AQUA_DECLARE_BINDABLES(CONTROLLER_BINDABLES)
	#undef CONTROLLER_BINDABLES

	// Per-frame controller update entry point.
	void Update(Entity&, float) override;

protected:
	// Allows game-specific controllers to customize vertical acceleration while
	// retaining the shared collision and grounding implementation.
	virtual float GravityScale() const { return 1.0f; }
	virtual float MaxWalkableSlopeAngle() const { return 45.0f; }

	// Internal helpers used by the controller update pipeline.
	void SetDiagnosticInput(const glm::vec3& input) { m_diagnosticInput = input; }
	void MoveWithCollisions(Entity& owner, const glm::vec3& desiredMovement, float dt);
	void Move(Entity& owner, const glm::vec2& direction, float dt);

	// Tunable movement settings.
	float m_moveSpeed = 50.0f;

	// Runtime movement state.
	glm::vec3 m_movementDirection{ 0.0f };
	glm::vec3 m_pendingMovement{ 0.0f };
	glm::vec3 m_velocity{ 0.0f };
	float m_groundedLossTimer = 0.0f;
	bool m_grounded = false;
	bool m_isGrounded = false;
	bool m_isMoving = false;
	glm::vec3 m_groundNormal{ 0.0f, 1.0f, 0.0f };

	// Diagnostics and editor-facing helper data.
	glm::vec3 m_diagnosticInput{ 0.0f };
};


