#pragma once

#include "Engine/Component.h"
#include "glm/glm.hpp"

class Entity;

class Controller final : public Component {
public:
	Controller() = default;

	const char* Name() const override { return "Controller"; }

	float MoveSpeed() const { return m_moveSpeed; }
	void SetMoveSpeed(float moveSpeed) { m_moveSpeed = moveSpeed; }

	void SetOwner(Entity* owner) { m_owner = owner; }
	Entity* Owner() const { return m_owner; }

	void Update(Entity&, float) override;

private:
	Entity* m_owner = nullptr;
	float m_moveSpeed = 50.0f;
};

