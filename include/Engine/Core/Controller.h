#pragma once

#include "Engine/Core/Component.h"
#include "glm/glm.hpp"

class Entity;

class Controller : public Component {
public:
	Controller() = default;

	const char* Name() const override { return "Controller"; }
	int ExecutionOrder() const override { return -100; }

	float MoveSpeed() const { return m_moveSpeed; }
	void SetMoveSpeed(float moveSpeed) { m_moveSpeed = moveSpeed; }
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

	float m_moveSpeed = 50.0f;
	bool m_isMoving = false;
	float m_movementDeadzone = 0.01f;
	glm::vec3 m_movementDirection{ 0.0f };
	glm::vec3 m_diagnosticInput{ 0.0f };
};


