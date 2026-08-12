#pragma once

#include "Engine/Core/PhysicsCollider.h"
#include "Engine/Core/Physics.h"

#include <vector>

class Scene;

class PhysicsWorld final
{
public:
	static PhysicsWorld& Instance();

	// Rebuilds the collision representation for one active scene. Existing
	// records are cleared first, then each scene entity is passed to Add(),
	// which excludes meshless entities and entities without a valid world AABB.
	void RegisterScene(const Scene& scene);

	// Finds the active collider record associated with an entity.
	ColliderHandle Find(const Entity& entity) const;

	// Sweeps a moving collider through the cached world representation and
	// returns the earliest collision found against enabled collider records.
	// The caller supplies the moving shape's current bounds and movement; this
	// method only discovers a hit and does not move entities or resolve sliding.
	// When supplied, hitEntity receives the owner of the earliest hit collider.
	Physics::SweepCollision Sweep(
		ColliderHandle movingCollider,
		const glm::vec3& minBounds,
		const glm::vec3& maxBounds,
		const glm::vec3& movement,
		Entity** hitEntity = nullptr) const;

	// Add only accepts entities with a mesh and a valid world AABB.
	// The returned handle is InvalidColliderHandle when registration fails.
	ColliderHandle Add(Entity& entity);

	void Remove(ColliderHandle handle);
	// Refreshes an entity's cached collider state when it has moved or changed
	// shape. This overload keeps callers from managing temporary handles.
	void Update(Entity& entity);

	// Refreshes the entity's cached bounds and shape without changing its handle.
	// Invalid or no-longer-collidable entities are disabled in world storage.
	void Update(ColliderHandle handle);

	void Clear();

	const std::vector<PhysicsCollider>& Colliders() const;

private:
	PhysicsWorld() = default;

	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;

	std::vector<PhysicsCollider> m_colliders;
};
