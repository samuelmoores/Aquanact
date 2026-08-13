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

	// Sweeps the camera's sphere through registered colliders without moving it.
	// This is a continuous movement query: it finds whether the camera hits an
	// obstacle while moving and returns the earliest contact details.
	// Entities marked IgnoreCameraCollision() are excluded; an optional hitEntity
	// receives the owner of the earliest blocking collider.
	Physics::SweepCollision SweepCamera(
		const glm::vec3& position,
		float radius,
		const glm::vec3& movement,
		Entity** hitEntity = nullptr) const;
	// Runs the same shape-aware camera sweep against one entity. This is used to
	// find the physical surface of the followed target before sweeping the boom.
	Physics::SweepCollision SweepCameraAgainst(
		const glm::vec3& position,
		float radius,
		const glm::vec3& movement,
		const Entity& entity) const;

	// Tests whether the camera sphere overlaps any registered collider at one
	// position, using the same camera-ignore filter as SweepCamera.
	// Unlike SweepCamera, this is an instantaneous position query with no
	// movement or collision time; it is used to validate safe camera positions.
	bool OverlapsCamera(
		const glm::vec3& position,
		float radius) const;
	// Tests the segment from cameraPosition to targetPosition against each
	// blocker's box, capsule, or convex shape. The target itself and non-blocking
	// entities are ignored; true means no other collider obscures the target.
	bool HasCameraLineOfSight(
		const glm::vec3& cameraPosition,
		const glm::vec3& targetPosition,
		const Entity* target) const;

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
