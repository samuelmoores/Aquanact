#pragma once

#include "Engine/Component.h"
#include "glm/glm.hpp"

class Entity;

class Controller final : public Component {
public:
	Controller() = default;

	const char* Name() const override { return "Controller"; }
	int ExecutionOrder() const override { return -100; }

	float MoveSpeed() const { return m_moveSpeed; }
	void SetMoveSpeed(float moveSpeed) { m_moveSpeed = moveSpeed; }
	bool IsMoving() const { return m_isMoving; }
	std::vector<BindableMember> GetBindableMembers() const override;
	bool TryGetBindableValue(const std::string& memberName, float& value) const override;

	void Update(Entity&, float) override;

private:
	float m_moveSpeed = 50.0f;
	bool m_isMoving = false;
	float m_movementDeadzone = 0.01f;
};

