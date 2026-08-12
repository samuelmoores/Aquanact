#include "Engine/Core/PhysicsWorld.h"

#include "Engine/Core/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	void BuildVerticalCapsule(const glm::vec3& boxMin, const glm::vec3& boxMax,
		glm::vec3& base, glm::vec3& tip, float& radius)
	{
		const glm::vec3 center = (boxMin + boxMax) * 0.5f;
		const glm::vec3 halfExtents = (boxMax - boxMin) * 0.5f;
		radius = glm::min(glm::min(halfExtents.x, halfExtents.z), halfExtents.y);
		radius = glm::max(radius, 0.001f);

		const float baseY = boxMin.y + radius;
		const float tipY = boxMax.y - radius;
		base = glm::vec3(center.x, glm::min(baseY, tipY), center.z);
		tip = glm::vec3(center.x, glm::max(baseY, tipY), center.z);
	}

	std::vector<Physics::ConvexPlane> BuildConvexPlanes(const PhysicsCollider& collider)
	{
		// A convex collider needs the owning mesh and its current world transform.
		// Invalid or incomplete meshes cannot provide a reliable plane set.
		Entity* object = collider.owner;
		const Mesh* mesh = object ? object->GetMesh() : nullptr;
		if (!object || !mesh || mesh->Faces().size() < 3 || mesh->Vertices().empty())
		{
			return {};
		}

		const auto& vertices = mesh->Vertices();
		const glm::mat4 model = object->BuildModelMatrix();

		// Sample at most 256 vertices to make the face-orientation check bounded
		// even when the source mesh contains a large number of vertices.
		const std::size_t pointCount = std::min<std::size_t>(vertices.size(), 256);
		std::vector<glm::vec3> points;
		points.reserve(pointCount);
		for (std::size_t i = 0; i < pointCount; ++i)
		{
			const std::size_t sourceIndex = i * vertices.size() / pointCount;
			points.push_back(glm::vec3(model * glm::vec4(vertices[sourceIndex].position, 1.0f)));
		}

		// Transform every mesh vertex once into world space so face calculations
		// below do not repeatedly rebuild the entity model matrix.
		std::vector<glm::vec3> worldVertices;
		worldVertices.reserve(vertices.size());
		for (const Vertex3D& vertex : vertices)
		{
			worldVertices.push_back(glm::vec3(model * glm::vec4(vertex.position, 1.0f)));
		}

		std::vector<Physics::ConvexPlane> planes;
		const auto& faces = mesh->Faces();
		for (std::size_t face = 0; face + 2 < faces.size(); face += 3)
		{
			// Faces are stored as triangle indices. Ignore incomplete or invalid
			// triangles rather than indexing outside the transformed vertex array.
			if (faces[face] >= worldVertices.size() || faces[face + 1] >= worldVertices.size() || faces[face + 2] >= worldVertices.size())
			{
				continue;
			}

			const glm::vec3 a = worldVertices[faces[face]];
			const glm::vec3 b = worldVertices[faces[face + 1]];
			const glm::vec3 c = worldVertices[faces[face + 2]];
			// The cross product gives the triangle normal before normalization.
			glm::vec3 normal = glm::cross(b - a, c - a);
			const float normalLength = glm::length(normal);
			if (normalLength <= 1e-5f)
			{
				continue;
			}

			normal /= normalLength;
			float distance = glm::dot(normal, a);
			float maximum = -std::numeric_limits<float>::max();
			float minimum = std::numeric_limits<float>::max();
			// Check the sampled mesh points against the face so internal triangle
			// surfaces can be rejected and the plane can be oriented consistently.
			for (const glm::vec3& point : points)
			{
				const float signedDistance = glm::dot(normal, point) - distance;
				maximum = std::max(maximum, signedDistance);
				minimum = std::min(minimum, signedDistance);
			}

			if (maximum > 0.01f && minimum < -0.01f)
			{
				continue;
			}
			if (maximum > 0.01f)
			{
				// Reverse planes whose normal points away from the convex volume.
				normal = -normal;
				distance = -distance;
			}

			// Imported meshes commonly contain multiple coplanar triangles. Keep
			// one plane for each unique normal/distance pair.
			bool duplicate = false;
			for (const Physics::ConvexPlane& existing : planes)
			{
				if (glm::dot(existing.normal, normal) > 0.999f && std::abs(existing.distance - distance) < 0.01f)
				{
					duplicate = true;
					break;
				}
			}
			if (!duplicate)
			{
				planes.push_back({ normal, distance });
			}
		}

		return planes;
	}
}

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

ColliderHandle PhysicsWorld::Find(const Entity& entity) const
{
	for (ColliderHandle handle = 0; handle < m_colliders.size(); ++handle)
	{
		const PhysicsCollider& collider = m_colliders[handle];
		if (collider.enabled && collider.owner == &entity)
		{
			return handle;
		}
	}

	return InvalidColliderHandle;
}

Physics::SweepCollision PhysicsWorld::Sweep(
	ColliderHandle movingCollider,
	const glm::vec3& minBounds,
	const glm::vec3& maxBounds,
	const glm::vec3& movement,
	Entity** hitEntity) const
{
	// This method discovers a contact; the caller remains responsible for
	// applying movement and resolving the resulting slide.
	if (hitEntity)
	{
		*hitEntity = nullptr;
	}

	Physics::SweepCollision earliestHit;
	if (movingCollider >= m_colliders.size())
	{
		return earliestHit;
	}

	// Read the moving record once so every candidate uses the same shape data.
	const PhysicsCollider& moving = m_colliders[movingCollider];
	if (!moving.enabled)
	{
		return earliestHit;
	}

	// Build a cheap broadphase volume covering the shape's complete movement.
	// This rejects distant records before invoking narrow-phase geometry code.
	const glm::vec3 sweptMin = glm::min(minBounds, minBounds + movement);
	const glm::vec3 sweptMax = glm::max(maxBounds, maxBounds + movement);

	// Initialize Capsule
	const bool useCapsule = moving.shape == PhysicsColliderShape::Capsule;
	glm::vec3 capsuleBase;
	glm::vec3 capsuleTip;

	if (useCapsule)
	{
		const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
		capsuleBase = center - glm::vec3(0.0f, moving.capsuleHalfLength, 0.0f);
		capsuleTip = center + glm::vec3(0.0f, moving.capsuleHalfLength, 0.0f);
	}

	// Query only the dedicated collision storage, not the full scene or entity
	// component graph.
	for (ColliderHandle handle = 0; handle < m_colliders.size(); ++handle)
	{
		if (handle == movingCollider)
		{
			continue;
		}

		const PhysicsCollider& candidate = m_colliders[handle];
		if (!candidate.enabled || !candidate.owner)
		{
			continue;
		}

		// Broadphase rejection avoids invoking shape-specific geometry code for
		// colliders that cannot intersect the moving shape's swept bounds.
		if (sweptMax.x < candidate.minBounds.x || sweptMin.x > candidate.maxBounds.x ||
			sweptMax.y < candidate.minBounds.y || sweptMin.y > candidate.maxBounds.y ||
			sweptMax.z < candidate.minBounds.z || sweptMin.z > candidate.maxBounds.z)
		{
			continue;
		}

		// The broadphase passed, so now dispatch to the candidate's narrow phase.
		Physics::SweepCollision hit;
		if (candidate.shape == PhysicsColliderShape::Convex)
		{
			// Convex candidates are represented by their outward-facing planes.
			// Build those planes from the candidate's world-space mesh geometry.
			std::vector<Physics::ConvexPlane> planes = BuildConvexPlanes(candidate);

			// The convex sweep receives a point moving through expanded planes. The
			// point is the moving shape's center, while its support distance is added
			// to each plane below to account for the shape's size.
			const glm::vec3 movingCenter = useCapsule
				? (capsuleBase + capsuleTip) * 0.5f
				: (minBounds + maxBounds) * 0.5f;
			const glm::vec3 movingHalfExtents = (maxBounds - minBounds) * 0.5f;
			const float capsuleHalfLength = useCapsule ? moving.capsuleHalfLength : 0.0f;

			// Expand every plane by the moving shape's support distance. Capsules
			// use radius plus axial length; boxes use projected half-extents.
			for (Physics::ConvexPlane& plane : planes)
			{
				const float support = useCapsule
					? moving.capsuleRadius + capsuleHalfLength * std::abs(plane.normal.y)
					: glm::dot(glm::abs(plane.normal), movingHalfExtents);

				plane.distance += support;
			}

			// Sweep the center through the expanded convex volume. The result uses
			// the same normalized time interval as the box and capsule sweeps.
			hit = Physics::GetConvexSweep(planes, movingCenter, movement);
		}
		else if (useCapsule)
		{
			hit = Physics::GetCapsuleAABBSweep(
				capsuleBase, capsuleTip, moving.capsuleRadius, movement,
				candidate.minBounds, candidate.maxBounds);
		}
		else
		{
			hit = Physics::GetAABBSweep(
				minBounds, maxBounds, movement,
				candidate.minBounds, candidate.maxBounds);
		}

		// Multiple records may be touched by one movement. Keep the first contact
		// so the caller resolves against the nearest surface.
		if (hit.hit && hit.time < earliestHit.time)
		{
			earliestHit = hit;
			if (hitEntity)
			{
				// Keep the entity paired with the collision result whenever a nearer
				// candidate replaces the previous earliest hit.
				*hitEntity = candidate.owner;
			}
		}
	}

	return earliestHit;
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
	if (collider.shape == PhysicsColliderShape::Capsule)
	{
		glm::vec3 capsuleBase;
		glm::vec3 capsuleTip;
		BuildVerticalCapsule(minBounds, maxBounds, capsuleBase, capsuleTip, collider.capsuleRadius);
		collider.capsuleHalfLength = glm::length(capsuleTip - capsuleBase) * 0.5f;
	}
	collider.isStatic = entity.GetController() == nullptr;

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
	if (collider.isStatic)
	{
		return;
	}

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

void PhysicsWorld::Update(Entity& entity)
{
	const ColliderHandle handle = Find(entity);
	if (handle != InvalidColliderHandle)
	{
		Update(handle);
	}
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
