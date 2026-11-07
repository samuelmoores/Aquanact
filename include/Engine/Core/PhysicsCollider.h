#pragma once

#include "Engine/Core/Entity.h"
#include "Engine/Core/Physics.h"

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

class LevelCollider;

struct PhysicsCollider
{
	// The world does not own the entity; this is used to identify collision results.
	Entity* owner = nullptr;
	LevelCollider* levelOwner = nullptr;

	PhysicsColliderShape shape = PhysicsColliderShape::Box;

	glm::vec3 minBounds{ 0.0f };
	glm::vec3 maxBounds{ 0.0f };
	// Capsule dimensions remain stable when the visual entity rotates.
	float capsuleRadius = 0.0f;
	glm::vec3 capsuleRadii{ 0.0f };
	float capsuleHalfLength = 0.0f;
	glm::vec3 capsuleAxis{ 0.0f, 1.0f, 0.0f };
	float sphereRadius = 0.0f;
	// Convex mesh planes are derived from the current world transform and reused
	// by overlap and sweep queries until this collider is refreshed.
	std::vector<Physics::ConvexPlane> convexPlanes;

	bool isStatic = true;
	bool enabled = true;
	bool isHitbox = false;
};

using ColliderHandle = std::size_t;

constexpr ColliderHandle InvalidColliderHandle =
	static_cast<ColliderHandle>(-1);
