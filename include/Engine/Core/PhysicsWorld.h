#pragma once

#include "Engine/Core/PhysicsCollider.h"

#include <vector>

class PhysicsWorld final
{
public:
	static PhysicsWorld& Instance();

	// Add only accepts entities with a mesh and a valid world AABB.
	// The returned handle is InvalidColliderHandle when registration fails.
	ColliderHandle Add(Entity& entity);
	void Remove(ColliderHandle handle);
	void Update(ColliderHandle handle);
	void Clear();

	const std::vector<PhysicsCollider>& Colliders() const;

private:
	PhysicsWorld() = default;

	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;

	std::vector<PhysicsCollider> m_colliders;
};
