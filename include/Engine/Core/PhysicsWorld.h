#pragma once

#include "Engine/Core/PhysicsCollider.h"

#include <vector>

class PhysicsWorld final
{
public:
	static PhysicsWorld& Instance();

private:
	PhysicsWorld() = default;

	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;

	std::vector<PhysicsCollider> m_colliders;
};
