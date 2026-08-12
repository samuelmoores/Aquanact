#pragma once

#include "Engine/Core/Entity.h"

#include <cstddef>
#include <glm/glm.hpp>

struct PhysicsCollider
{
	// The world does not own the entity; this is used to identify collision results.
	Entity* owner = nullptr;

	PhysicsColliderShape shape = PhysicsColliderShape::Box;

	glm::vec3 minBounds{ 0.0f };
	glm::vec3 maxBounds{ 0.0f };
	// Capsule dimensions remain stable when the visual entity rotates.
	float capsuleRadius = 0.0f;
	float capsuleHalfLength = 0.0f;

	bool isStatic = true;
	bool enabled = true;
};

using ColliderHandle = std::size_t;

constexpr ColliderHandle InvalidColliderHandle =
	static_cast<ColliderHandle>(-1);
