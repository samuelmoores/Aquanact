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
	float GroundAcceleration() const { return m_groundAcceleration; }
	void SetGroundAcceleration(float acceleration) { m_groundAcceleration = acceleration; }
	float AirAcceleration() const { return m_airAcceleration; }
	void SetAirAcceleration(float acceleration) { m_airAcceleration = acceleration; }
	float GroundFriction() const { return m_groundFriction; }
	void SetGroundFriction(float friction) { m_groundFriction = friction; }
	float AirDrag() const { return m_airDrag; }
	void SetAirDrag(float drag) { m_airDrag = drag; }
	float MovementDeadzone() const { return m_movementDeadzone; }
	void SetMovementDeadzone(float deadzone) { m_movementDeadzone = deadzone; }
	bool IsMoving() const { return m_isMoving; }
	std::vector<BindableMember> GetBindableMembers() const override;
	bool TryGetBindableValue(const std::string& memberName, float& value) const override;
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
	float m_groundAcceleration = 4000.0f;
	float m_airAcceleration = 800.0f;
	float m_groundFriction = 5000.0f;
	float m_airDrag = 0.1f;
	glm::vec3 m_velocity{ 0.0f };
	bool m_grounded = false;
	bool m_isMoving = false;
	float m_movementDeadzone = 0.01f;
	glm::vec3 m_movementDirection{ 0.0f };
	glm::vec3 m_diagnosticInput{ 0.0f };
};


