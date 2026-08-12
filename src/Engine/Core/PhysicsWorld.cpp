#include "Engine/Core/PhysicsWorld.h"

#include "Engine/Core/Scene.h"

PhysicsWorld& PhysicsWorld::Instance()
{
	static PhysicsWorld world;
	return world;
}

void PhysicsWorld::RegisterScene(const Scene& scene)
{
	// The world represents one active scene, so discard records belonging to
	// the previous scene before registering the new scene's entities.
	Clear();

	for (const auto& object : scene.Objects())
	{
		if (object)
		{
			Add(*object);
		}
	}
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

void PhysicsWorld::Update(ColliderHandle handle)
{
	// Ignore stale handles instead of allowing an invalid index to access the
	// packed collider storage.
	if (handle >= m_colliders.size())
	{
		return;
	}

	PhysicsCollider& collider = m_colliders[handle];
	if (!collider.owner || !collider.owner->GetMesh())
	{
		collider.enabled = false;
		return;
	}

	glm::vec3 minBounds;
	glm::vec3 maxBounds;
	if (!collider.owner->WorldAABB(minBounds, maxBounds))
	{
		collider.enabled = false;
		return;
	}

	// Refresh the cached collision representation without changing the handle.
	collider.shape = collider.owner->GetPhysicsColliderShape();
	collider.minBounds = minBounds;
	collider.maxBounds = maxBounds;
	collider.enabled = true;
}

void PhysicsWorld::Remove(ColliderHandle handle)
{
	if (handle >= m_colliders.size())
	{
		return;
	}

	// Keep the vector index stable so removing one collider does not invalidate
	// handles belonging to other colliders. Erasing this entry would shift every
	// later collider left by one position, making their index-based handles point
	// at the wrong entities.
	//
	// An inactive slot uses a little extra storage, but it keeps removal simple
	// and safe while the world is still using vector indices as handles. A later
	// generation-based handle or free-list can reclaim these slots if needed.
	m_colliders[handle] = PhysicsCollider{};
	m_colliders[handle].enabled = false;
}

void PhysicsWorld::Clear()
{
	// Level changes invalidate every entity pointer and cached bounds in the
	// current collision world, so discard all records together.
	m_colliders.clear();
}

const std::vector<PhysicsCollider>& PhysicsWorld::Colliders() const
{
	// Expose read-only storage so query code can inspect colliders without
	// bypassing PhysicsWorld's registration and update methods.
	return m_colliders;
}
