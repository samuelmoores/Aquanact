#include "Engine/Core/PhysicsWorld.h"

PhysicsWorld& PhysicsWorld::Instance()
{
	static PhysicsWorld world;
	return world;
}

ColliderHandle PhysicsWorld::Add(Entity& entity)
{
	// Meshless entities have no geometry that can participate in collision.
	if (!entity.GetMesh())
	{
		return InvalidColliderHandle;
	}

	glm::vec3 minBounds;
	glm::vec3 maxBounds;
	// Store a world-space bounds snapshot so queries do not need to inspect
	// unrelated entity data just to perform broadphase checks.
	if (!entity.WorldAABB(minBounds, maxBounds))
	{
		return InvalidColliderHandle;
	}

	// Copy the collision-facing state into the PhysicsWorld-owned record while
	// retaining the entity pointer only for identifying future collision results.
	PhysicsCollider collider;
	collider.owner = &entity;
	collider.shape = entity.GetPhysicsColliderShape();
	collider.minBounds = minBounds;
	collider.maxBounds = maxBounds;

	m_colliders.push_back(collider);
	return m_colliders.size() - 1;
}
