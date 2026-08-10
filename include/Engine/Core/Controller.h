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

	float MoveSpeed() const { return m_moveSpeed; }
	void SetMoveSpeed(float moveSpeed) { m_moveSpeed = moveSpeed; }
	bool IsGrounded() const { return m_isGrounded; }
	float MovementDeadzone() const { return m_movementDeadzone; }
	void SetMovementDeadzone(float deadzone) { m_movementDeadzone = deadzone; }
	bool IsMoving() const { return m_isMoving; }

	// Bindable values are sampled state, not events. The UI polls them whenever
	// it needs the latest controller state.
	#define CONTROLLER_BINDABLES(VALUE, FUNCTION) \
		VALUE(m_isMoving) \
		VALUE(m_isGrounded) \
		VALUE(m_moveSpeed)
	AQUA_DECLARE_BINDABLES(CONTROLLER_BINDABLES)
	#undef CONTROLLER_BINDABLES
	void SetMovementDirection(const glm::vec3& direction);
	void StopMoving();
	const glm::vec3& MovementDirection() const { return m_movementDirection; }

	void Update(Entity&, float) override;

	protected:
	void SetDiagnosticInput(const glm::vec3& input) { m_diagnosticInput = input; }
	void ApplyMovement(Entity& owner, float dt);
	glm::vec3 MoveWithPhysics(Entity& owner, const glm::vec3& desiredHorizontalVelocity, float dt);
	glm::vec3 MoveWithCollision(Entity& owner, const glm::vec3& delta,
		glm::vec3* lastCollisionNormal = nullptr, bool* collidedWithGround = nullptr);

	float m_moveSpeed = 50.0f;
	glm::vec3 m_velocity{ 0.0f };
	bool m_grounded = false;
	bool m_isGrounded = false;
	float m_groundedLossTimer = 0.0f;
	bool m_isMoving = false;
	float m_movementDeadzone = 0.01f;
	glm::vec3 m_movementDirection{ 0.0f };
	glm::vec3 m_diagnosticInput{ 0.0f };
};


