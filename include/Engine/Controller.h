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

	void SetRegistered(bool registered) { m_registered = registered; }
	bool Registered() const { return m_registered; }

	void Update(Entity&, float) override;

private:
	bool m_registered = false;
	float m_moveSpeed = 50.0f;
};

