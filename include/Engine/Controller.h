#pragma once

#include "Engine/Component.h"
#include "glm/glm.hpp"

class Object3D;

class Controller final : public Component {
public:
	Controller() = default;

	const char* Name() const override { return "Controller"; }

	float MoveSpeed() const { return m_moveSpeed; }
	void SetMoveSpeed(float moveSpeed) { m_moveSpeed = moveSpeed; }

	void SetOwner(Object3D* owner) { m_owner = owner; }
	Object3D* Owner() const { return m_owner; }

	void Update(Object3D&, float) override;

private:
	Object3D* m_owner = nullptr;
	float m_moveSpeed = 50.0f;
};

