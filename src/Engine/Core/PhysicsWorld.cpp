#include "Engine/Core/PhysicsWorld.h"

#include "Engine/Core/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	bool IsCameraCollisionCandidate(const PhysicsCollider& collider)
	{
		return collider.enabled && collider.owner &&
			!collider.owner->IgnoreCameraCollision();
	}

	bool IsCameraCollisionCandidate(
		const PhysicsCollider& collider, const Entity* ignoredEntity)
	{
		return IsCameraCollisionCandidate(collider) &&
			collider.owner != ignoredEntity;
	}

	bool OverlapsSweptBounds(const glm::vec3& sweptMin, const glm::vec3& sweptMax,
		const PhysicsCollider& collider)
	{
		return !(sweptMax.x < collider.minBounds.x || sweptMin.x > collider.maxBounds.x ||
			sweptMax.y < collider.minBounds.y || sweptMin.y > collider.maxBounds.y ||
			sweptMax.z < collider.minBounds.z || sweptMin.z > collider.maxBounds.z);
	}

	bool SegmentIntersectsBounds(const glm::vec3& start, const glm::vec3& end,
		const glm::vec3& boundsMin, const glm::vec3& boundsMax)
	{
		const glm::vec3 movement = end - start;
		float enterTime = 0.0f;
		float exitTime = 1.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::abs(movement[axis]) <= 1e-6f)
			{
				if (start[axis] < boundsMin[axis] || start[axis] > boundsMax[axis])
				{
					return false;
				}
				continue;
			}

			float nearTime = (boundsMin[axis] - start[axis]) / movement[axis];
			float farTime = (boundsMax[axis] - start[axis]) / movement[axis];
			if (nearTime > farTime)
			{
				std::swap(nearTime, farTime);
			}
			enterTime = glm::max(enterTime, nearTime);
			exitTime = glm::min(exitTime, farTime);
			if (enterTime > exitTime)
			{
				return false;
			}
		}
		// A segment that only grazes one edge or corner has no interval inside the
		// box and should not cause line-of-sight flicker.
		return exitTime - enterTime > 1e-5f;
	}

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

	bool CameraOverlapsCollider(
		const glm::vec3& position,
		float radius,
		const PhysicsCollider& collider);

	Physics::SweepCollision SweepCameraAgainstCollider(
		const glm::vec3& position,
		float radius,
		const glm::vec3& movement,
		const PhysicsCollider& collider)
	{
		if (CameraOverlapsCollider(position, radius, collider))
		{
			Physics::SweepCollision overlap;
			if (glm::length(movement) > 1e-6f)
			{
				overlap.hit = true;
				overlap.normal = -glm::normalize(movement);
				overlap.time = 0.0f;
			}
			return overlap;
		}

		switch (collider.shape)
		{
		case PhysicsColliderShape::Capsule:
		{
			glm::vec3 base;
			glm::vec3 tip;
			float capsuleRadius = 0.0f;
			BuildVerticalCapsule(
				collider.minBounds, collider.maxBounds, base, tip, capsuleRadius);
			return Physics::GetSphereCapsuleSweep(
				position, radius, movement, base, tip, capsuleRadius);
		}
		case PhysicsColliderShape::Convex:
		{
			std::vector<Physics::ConvexPlane> planes = BuildConvexPlanes(collider);
			if (planes.empty())
			{
				return Physics::GetSphereAABBSweep(
					position, radius, movement,
					collider.minBounds, collider.maxBounds);
			}
			for (Physics::ConvexPlane& plane : planes)
			{
				plane.distance += radius;
			}
			return Physics::GetConvexSweep(planes, position, movement);
		}
		case PhysicsColliderShape::Box:
		default:
			return Physics::GetSphereAABBSweep(
				position, radius, movement, collider.minBounds, collider.maxBounds);
		}
	}

	bool CameraOverlapsCollider(
		const glm::vec3& position,
		float radius,
		const PhysicsCollider& collider)
	{
		switch (collider.shape)
		{
		case PhysicsColliderShape::Capsule:
		{
			glm::vec3 base;
			glm::vec3 tip;
			float capsuleRadius = 0.0f;
			BuildVerticalCapsule(
				collider.minBounds, collider.maxBounds, base, tip, capsuleRadius);
			return Physics::SphereCapsuleOverlap(
				position, radius, base, tip, capsuleRadius);
		}
		case PhysicsColliderShape::Convex:
		{
			const std::vector<Physics::ConvexPlane> planes =
				BuildConvexPlanes(collider);
			if (planes.empty())
			{
				return Physics::SphereAABBOverlap(
					position, radius, collider.minBounds, collider.maxBounds);
			}
			for (const Physics::ConvexPlane& plane : planes)
			{
				if (glm::dot(plane.normal, position) >
					plane.distance + radius + 1e-5f)
				{
					return false;
				}
			}
			return true;
		}
		case PhysicsColliderShape::Box:
		default:
			return Physics::SphereAABBOverlap(
				position, radius, collider.minBounds, collider.maxBounds);
		}
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

Physics::SweepCollision PhysicsWorld::SweepCamera(
	const glm::vec3& position,
	float radius,
	const glm::vec3& movement,
	Entity** hitEntity,
	const Entity* ignoredEntity) const
{
	// The camera owns position resolution, so this query only reports the
	// nearest blocking contact and optionally identifies its entity.
	if (hitEntity)
	{
		*hitEntity = nullptr;
	}

	Physics::SweepCollision earliestHit;

	// Treat the camera as a sphere and build a bounds volume covering its entire
	// movement. This is the cheap broadphase volume for candidate filtering.
	const glm::vec3 sphereRadius(radius);
	const glm::vec3 sweptMin = glm::min(position, position + movement) - sphereRadius;
	const glm::vec3 sweptMax = glm::max(position, position + movement) + sphereRadius;

	// Query only PhysicsWorld's collision records. Camera filtering is applied
	// before any geometry calculation so excluded entities are never tested.
	for (const PhysicsCollider& candidate : m_colliders)
	{
		if (!IsCameraCollisionCandidate(candidate, ignoredEntity))
		{
			continue;
		}

		// Reject candidates outside the camera sphere's swept broadphase bounds.
		if (!OverlapsSweptBounds(sweptMin, sweptMax, candidate))
		{
			continue;
		}

		// Dispatch to the candidate's actual shape after broadphase rejection.
		const Physics::SweepCollision hit = SweepCameraAgainstCollider(
			position, radius, movement, candidate);
		if (hit.hit && glm::dot(movement, hit.normal) >= -1e-5f)
		{
			// A sweep can report a face while the sphere is tangent to it or moving
			// away from it. Those contacts do not block motion and are especially
			// likely to create zero-progress loops at box corners.
			continue;
		}

		// Several colliders may overlap the swept path. Keep the first contact so
		// the camera resolves against the nearest obstruction.
		if (hit.hit && hit.time < earliestHit.time)
		{
			earliestHit = hit;
			if (hitEntity)
			{
				*hitEntity = candidate.owner;
			}
		}
	}

	return earliestHit;
}

Physics::SweepCollision PhysicsWorld::SweepCameraAgainst(
	const glm::vec3& position,
	float radius,
	const glm::vec3& movement,
	const Entity& entity) const
{
	const ColliderHandle handle = Find(entity);
	if (handle == InvalidColliderHandle)
	{
		return {};
	}

	const PhysicsCollider& collider = m_colliders[handle];
	if (!collider.enabled || !collider.owner)
	{
		return {};
	}

	return SweepCameraAgainstCollider(position, radius, movement, collider);
}

bool PhysicsWorld::OverlapsCamera(
	const glm::vec3& position,
	float radius,
	const Entity* ignoredEntity) const
{
	// Check only the collision records that are valid for camera queries. This
	// keeps camera blocking behavior consistent with SweepCamera().
	for (const PhysicsCollider& candidate : m_colliders)
	{
		if (!IsCameraCollisionCandidate(candidate, ignoredEntity))
		{
			continue;
		}
		if (!Physics::SphereAABBOverlap(
			position, radius, candidate.minBounds, candidate.maxBounds))
		{
			continue;
		}

		// Match the sweep narrow phase so a validated position cannot disagree with
		// movement resolution about the shape of an object.
		if (CameraOverlapsCollider(position, radius, candidate))
		{
			// One blocking collider is enough to classify the position as blocked.
			return true;
		}
	}

	// No eligible collider overlaps the camera sphere at this position.
	return false;
}

bool PhysicsWorld::HasCameraLineOfSight(
	const glm::vec3& cameraPosition,
	const glm::vec3& targetPosition,
	const Entity* target) const
{
	// Build a finite ray from the camera to the target. The target distance is
	// used to ensure objects behind the player do not count as obstructions.
	const glm::vec3 ray = targetPosition - cameraPosition;
	const float distance = glm::length(ray);
	if (distance <= 0.0001f)
	{
		// Coincident positions are visible by definition because there is no
		// segment that another object could obstruct.
		return true;
	}

	for (const PhysicsCollider& candidate : m_colliders)
	{
		// Only enabled mesh colliders that explicitly block camera view can
		// obstruct this segment. The target is never considered an obstruction.
		if (!candidate.enabled || !candidate.owner || candidate.owner == target ||
			!candidate.owner->BlocksCameraView() || !candidate.owner->GetMesh())
		{
			continue;
		}

		// Use the AABB as a cheap broadphase, then test the candidate's actual
		// configured shape with a zero-radius finite sweep.
		if (!SegmentIntersectsBounds(cameraPosition, targetPosition,
			candidate.minBounds, candidate.maxBounds))
		{
			continue;
		}
		const Physics::SweepCollision hit = SweepCameraAgainstCollider(
			cameraPosition, 0.0f, ray, candidate);
		if (hit.hit && hit.time < 1.0f - 1e-5f)
		{
			return false;
		}
	}

	// No eligible collider intersected the finite camera-to-target segment.
	return true;
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
