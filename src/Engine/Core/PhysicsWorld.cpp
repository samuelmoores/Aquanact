#include "Engine/Core/PhysicsWorld.h"

#include "Engine/Core/Scene.h"
#include "Engine/Core/LevelCollider.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Debug.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <limits>
#include <unordered_set>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
	bool SameMatrix(const glm::mat4& left, const glm::mat4& right)
	{
		for (int column = 0; column < 4; ++column)
			for (int row = 0; row < 4; ++row)
				if (left[column][row] != right[column][row])
					return false;
		return true;
	}

	bool IsCameraCollisionCandidate(const PhysicsCollider& collider)
	{
		return collider.enabled && !collider.isHitbox &&
			((collider.owner && !collider.owner->IgnoreCameraCollision()) || collider.levelOwner);
	}

	bool IsCameraCollisionCandidate(
		const PhysicsCollider& collider, const Entity* ignoredEntity)
	{
		return IsCameraCollisionCandidate(collider) &&
			(collider.levelOwner || collider.owner != ignoredEntity);
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

	Physics::SweepCollision SweepVerticalCapsules(
		const glm::vec3& movingBase, const glm::vec3& movingTip, float movingRadius,
		const glm::vec3& movement, const glm::vec3& staticBase,
		const glm::vec3& staticTip, const glm::vec3& staticRadii)
	{
		const glm::vec3 combinedRadii = glm::max(staticRadii + glm::vec3(movingRadius), glm::vec3(0.001f));
		auto separationAt = [&](float time, glm::vec3* separation)
		{
			const glm::vec3 baseWorld = movingBase + movement * time;
			const glm::vec3 tipWorld = movingTip + movement * time;
			const glm::vec3 base = baseWorld / combinedRadii;
			const glm::vec3 tip = tipWorld / combinedRadii;
			const glm::vec3 normalizedStaticBase = staticBase / combinedRadii;
			const glm::vec3 normalizedStaticTip = staticTip / combinedRadii;
			const glm::vec3 u = tip - base;
			const glm::vec3 v = normalizedStaticTip - normalizedStaticBase;
			const glm::vec3 w = base - normalizedStaticBase;
			const float a = glm::dot(u, u), b = glm::dot(u, v), c = glm::dot(v, v);
			const float d = glm::dot(u, w), e = glm::dot(v, w);
			const float denominator = a * c - b * b;
			float s = denominator > 1e-6f ? std::clamp((b * e - c * d) / denominator, 0.0f, 1.0f) : 0.0f;
			float t = c > 1e-6f ? std::clamp((b * s + e) / c, 0.0f, 1.0f) : 0.0f;
			if (a > 1e-6f) s = std::clamp((b * t - d) / a, 0.0f, 1.0f);
			const glm::vec3 normalizedDelta = (base + u * s) - (normalizedStaticBase + v * t);
			if (separation) *separation = normalizedDelta;
			return glm::dot(normalizedDelta, normalizedDelta) - 1.0f;
		};

		auto makeHit = [&](float time)
		{
			Physics::SweepCollision hit;
			glm::vec3 separation;
			separationAt(time, &separation);
			if (glm::length(separation) <= 1e-5f)
				separation = glm::length(movement) > 1e-5f ? -glm::normalize(movement) : glm::vec3(0, 1, 0);
			hit.hit = true;
			hit.time = time;
			hit.normal = glm::normalize(separation / combinedRadii);
			return hit;
		};

		if (separationAt(0.0f, nullptr) <= 0.0f)
		{
			Physics::SweepCollision hit = makeHit(0.0f);
			return glm::dot(movement, hit.normal) < 0.0f ? hit : Physics::SweepCollision{};
		}
		constexpr int samples = 32;
		float previous = 0.0f;
		for (int sample = 1; sample <= samples; ++sample)
		{
			const float time = static_cast<float>(sample) / samples;
			if (separationAt(time, nullptr) > 0.0f) { previous = time; continue; }
			float low = previous, high = time;
			for (int iteration = 0; iteration < 12; ++iteration)
			{
				const float middle = (low + high) * 0.5f;
				if (separationAt(middle, nullptr) <= 0.0f) high = middle; else low = middle;
			}
			return makeHit(high);
		}
		return {};
	}

	std::vector<Physics::ConvexPlane> BuildConvexPlanes(
		const PhysicsCollider& collider, int submeshIndex = -1,
		bool* generatedThinPlane = nullptr)
	{
		if (generatedThinPlane)
			*generatedThinPlane = false;
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
		const auto& faces = mesh->Faces();
		std::size_t firstFaceIndex = 0;
		std::size_t lastFaceIndex = faces.size();
		if (submeshIndex >= 0)
		{
			firstFaceIndex = std::min<std::size_t>(mesh->FacesOffset(submeshIndex), faces.size());
			lastFaceIndex = std::min<std::size_t>(firstFaceIndex + mesh->FacesSize(submeshIndex), faces.size());
		}
		if (lastFaceIndex < firstFaceIndex + 3)
			return {};

		std::vector<std::uint32_t> pointIndices;
		if (submeshIndex >= 0)
		{
			std::unordered_set<std::uint32_t> seenPointIndices;
			for (std::size_t face = firstFaceIndex; face + 2 < lastFaceIndex; face += 3)
			{
				for (std::size_t corner = 0; corner < 3; ++corner)
				{
					const std::uint32_t vertexIndex = faces[face + corner];
					if (vertexIndex < vertices.size() && seenPointIndices.insert(vertexIndex).second)
					{
						pointIndices.push_back(vertexIndex);
					}
				}
			}
		}

		// Sample at most 256 vertices to make the face-orientation check bounded
		// even when the source mesh contains a large number of vertices.
		const std::size_t candidatePointCount = submeshIndex >= 0 ? pointIndices.size() : vertices.size();
		const std::size_t pointCount = std::min<std::size_t>(candidatePointCount, 256);
		if (pointCount == 0)
			return {};
		std::vector<glm::vec3> points;
		points.reserve(pointCount);
		for (std::size_t i = 0; i < pointCount; ++i)
		{
			const std::size_t sourceIndex = submeshIndex >= 0
				? pointIndices[i * pointIndices.size() / pointCount]
				: i * vertices.size() / pointCount;
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
		for (std::size_t face = firstFaceIndex; face + 2 < lastFaceIndex; face += 3)
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

		// A bounded 3D convex volume needs at least four distinct face planes.
		// Treat degenerate/open meshes as unavailable so callers can use their
		// existing AABB fallback instead of sweeping against an unbounded shape.
		if (planes.size() < 4)
		{
			// Thin architectural meshes are often authored as a single open plane.
			// Such geometry has no volume and therefore cannot provide four face
			// planes directly. Build a 2D convex outline and extrude it into a small
			// slab so it remains a real convex collider instead of falling back to
			// the mesh's potentially enormous AABB.
			glm::vec3 planeNormal(0.0f);
			for (std::size_t face = firstFaceIndex; face + 2 < lastFaceIndex; face += 3)
			{
				if (faces[face] >= worldVertices.size()
					|| faces[face + 1] >= worldVertices.size()
					|| faces[face + 2] >= worldVertices.size())
					continue;
				planeNormal = glm::cross(
					worldVertices[faces[face + 1]] - worldVertices[faces[face]],
					worldVertices[faces[face + 2]] - worldVertices[faces[face]]);
				if (glm::length(planeNormal) > 1e-5f)
					break;
			}
			const float normalLength = glm::length(planeNormal);
			if (normalLength <= 1e-5f)
				return {};
			planeNormal /= normalLength;

			std::vector<glm::vec3> surfacePoints;
			if (submeshIndex >= 0)
			{
				surfacePoints.reserve(pointIndices.size());
				for (const std::uint32_t index : pointIndices)
					if (index < worldVertices.size())
						surfacePoints.push_back(worldVertices[index]);
			}
			else
			{
				surfacePoints = worldVertices;
			}
			if (surfacePoints.size() < 3)
				return {};

			const glm::vec3 planeOrigin = surfacePoints.front();
			float minPlaneOffset = std::numeric_limits<float>::max();
			float maxPlaneOffset = -std::numeric_limits<float>::max();
			for (const glm::vec3& point : surfacePoints)
			{
				const float offset = glm::dot(point - planeOrigin, planeNormal);
				minPlaneOffset = std::min(minPlaneOffset, offset);
				maxPlaneOffset = std::max(maxPlaneOffset, offset);
			}
			const float meshScale = glm::length(collider.maxBounds - collider.minBounds);
			const float planarTolerance = std::max(0.05f, meshScale * 1e-5f);
			if (maxPlaneOffset - minPlaneOffset > planarTolerance)
				return {};

			const glm::vec3 slabCenter = planeOrigin
				+ planeNormal * ((minPlaneOffset + maxPlaneOffset) * 0.5f);
			const float halfThickness = std::max(
				1.0f, (maxPlaneOffset - minPlaneOffset) * 0.5f);
			const glm::vec3 referenceAxis = std::abs(planeNormal.y) < 0.9f
				? glm::vec3(0.0f, 1.0f, 0.0f)
				: glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 axisU = glm::normalize(glm::cross(referenceAxis, planeNormal));
			const glm::vec3 axisV = glm::cross(planeNormal, axisU);

			struct ProjectedPoint
			{
				float x;
				float y;
				glm::vec3 world;
			};
			std::vector<ProjectedPoint> projected;
			projected.reserve(surfacePoints.size());
			for (const glm::vec3& point : surfacePoints)
			{
				const glm::vec3 relative = point - slabCenter;
				projected.push_back({ glm::dot(relative, axisU), glm::dot(relative, axisV), point });
			}
			std::sort(projected.begin(), projected.end(), [](const ProjectedPoint& left, const ProjectedPoint& right)
			{
				return left.x == right.x ? left.y < right.y : left.x < right.x;
			});
			projected.erase(std::unique(projected.begin(), projected.end(), [](const ProjectedPoint& left, const ProjectedPoint& right)
			{
				return std::abs(left.x - right.x) < 1e-4f
					&& std::abs(left.y - right.y) < 1e-4f;
			}), projected.end());
			if (projected.size() < 3)
				return {};

			const auto cross2D = [](const ProjectedPoint& origin,
				const ProjectedPoint& a, const ProjectedPoint& b)
			{
				return (a.x - origin.x) * (b.y - origin.y)
					- (a.y - origin.y) * (b.x - origin.x);
			};
			std::vector<ProjectedPoint> hull;
			for (const ProjectedPoint& point : projected)
			{
				while (hull.size() >= 2
					&& cross2D(hull[hull.size() - 2], hull.back(), point) <= 1e-5f)
					hull.pop_back();
				hull.push_back(point);
			}
			const std::size_t lowerHullSize = hull.size();
			for (std::size_t i = projected.size() - 1; i-- > 0;)
			{
				const ProjectedPoint& point = projected[i];
				while (hull.size() > lowerHullSize
					&& cross2D(hull[hull.size() - 2], hull.back(), point) <= 1e-5f)
					hull.pop_back();
				hull.push_back(point);
			}
			if (hull.size() > 1)
				hull.pop_back();
			if (hull.size() < 3)
				return {};

			std::vector<Physics::ConvexPlane> thinPlanes;
			thinPlanes.push_back({ planeNormal, glm::dot(planeNormal, slabCenter) + halfThickness });
			thinPlanes.push_back({ -planeNormal, glm::dot(-planeNormal, slabCenter) + halfThickness });
			for (std::size_t i = 0; i < hull.size(); ++i)
			{
				const ProjectedPoint& start = hull[i];
				const ProjectedPoint& end = hull[(i + 1) % hull.size()];
				glm::vec3 sideNormal = axisU * (end.y - start.y)
					- axisV * (end.x - start.x);
				const float sideLength = glm::length(sideNormal);
				if (sideLength <= 1e-5f)
					continue;
				sideNormal /= sideLength;
				thinPlanes.push_back({ sideNormal, glm::dot(sideNormal, start.world) });
			}
			if (thinPlanes.size() < 5)
				return {};

			if (generatedThinPlane)
				*generatedThinPlane = true;
			return thinPlanes;
		}

		return planes;
	}

	std::vector<std::vector<Physics::ConvexPlane>> BuildPlanarConvexParts(
		const PhysicsCollider& collider, int submeshIndex)
	{
		Entity* object = collider.owner;
		const Mesh* mesh = object ? object->GetMesh() : nullptr;
		if (!object || !mesh)
			return {};

		const auto& vertices = mesh->Vertices();
		const auto& faces = mesh->Faces();
		const std::size_t firstFace = submeshIndex < 0 ? 0
			: std::min<std::size_t>(mesh->FacesOffset(submeshIndex), faces.size());
		const std::size_t lastFace = submeshIndex < 0 ? faces.size()
			: std::min<std::size_t>(firstFace + mesh->FacesSize(submeshIndex), faces.size());
		const glm::mat4 model = object->BuildModelMatrix();
		std::vector<glm::vec3> worldVertices;
		worldVertices.reserve(vertices.size());
		for (const Vertex3D& vertex : vertices)
			worldVertices.push_back(glm::vec3(model * glm::vec4(vertex.position, 1.0f)));

		struct PlanarGroup
		{
			glm::vec3 normal{ 0.0f };
			float distance = 0.0f;
			std::vector<glm::vec3> points;
		};
		std::vector<PlanarGroup> groups;
		const float planeTolerance = std::max(
			0.05f, glm::length(collider.maxBounds - collider.minBounds) * 1e-5f);
		for (std::size_t face = firstFace; face + 2 < lastFace; face += 3)
		{
			const std::uint32_t ia = faces[face];
			const std::uint32_t ib = faces[face + 1];
			const std::uint32_t ic = faces[face + 2];
			if (ia >= worldVertices.size() || ib >= worldVertices.size() || ic >= worldVertices.size())
				continue;
			const glm::vec3 a = worldVertices[ia];
			const glm::vec3 b = worldVertices[ib];
			const glm::vec3 c = worldVertices[ic];
			glm::vec3 normal = glm::cross(b - a, c - a);
			const float normalLength = glm::length(normal);
			if (normalLength <= 1e-5f)
				continue;
			normal /= normalLength;
			if (normal.x < -1e-5f || (std::abs(normal.x) <= 1e-5f && normal.y < -1e-5f)
				|| (std::abs(normal.x) <= 1e-5f && std::abs(normal.y) <= 1e-5f && normal.z < 0.0f))
				normal = -normal;
			const float distance = glm::dot(normal, a);
			PlanarGroup* group = nullptr;
			for (PlanarGroup& candidate : groups)
			{
				if (glm::dot(candidate.normal, normal) > 0.9999f
					&& std::abs(candidate.distance - distance) <= planeTolerance)
				{
					group = &candidate;
					break;
				}
			}
			if (!group)
			{
				groups.push_back({ normal, distance, {} });
				group = &groups.back();
			}
			group->points.push_back(a);
			group->points.push_back(b);
			group->points.push_back(c);
		}

		std::vector<std::vector<Physics::ConvexPlane>> parts;
		for (const PlanarGroup& group : groups)
		{
			if (group.points.size() < 3)
				continue;
			const glm::vec3 reference = std::abs(group.normal.y) < 0.9f
				? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			const glm::vec3 axisU = glm::normalize(glm::cross(reference, group.normal));
			const glm::vec3 axisV = glm::cross(group.normal, axisU);
			glm::vec3 center(0.0f);
			for (const glm::vec3& point : group.points)
				center += point;
			center /= static_cast<float>(group.points.size());
			float minU = std::numeric_limits<float>::max();
			float maxU = -std::numeric_limits<float>::max();
			float minV = std::numeric_limits<float>::max();
			float maxV = -std::numeric_limits<float>::max();
			for (const glm::vec3& point : group.points)
			{
				const glm::vec3 relative = point - center;
				const float u = glm::dot(relative, axisU);
				const float v = glm::dot(relative, axisV);
				minU = std::min(minU, u);
				maxU = std::max(maxU, u);
				minV = std::min(minV, v);
				maxV = std::max(maxV, v);
			}
			constexpr float halfThickness = 1.0f;
			std::vector<Physics::ConvexPlane> planes{
				{ group.normal, glm::dot(group.normal, center) + halfThickness },
				{ -group.normal, glm::dot(-group.normal, center) + halfThickness },
				{ axisU, glm::dot(axisU, center) + maxU },
				{ -axisU, glm::dot(-axisU, center) - minU },
				{ axisV, glm::dot(axisV, center) + maxV },
				{ -axisV, glm::dot(-axisV, center) - minV }
			};
			parts.push_back(std::move(planes));
		}
		return parts;
	}

	std::string ConvexColliderLabel(const PhysicsCollider& collider)
	{
		std::string label = collider.owner ? collider.owner->Name() : "<no owner>";
		if (collider.isBaseMeshCollider)
			return label + "/base-mesh";
		if (collider.owner && collider.owner->GetMesh() && collider.submeshIndex >= 0)
		{
			const std::string& name = collider.owner->GetMesh()->SubMeshName(collider.submeshIndex);
			label += "/" + (name.empty() ? "submesh-" : name + "/submesh-")
				+ std::to_string(collider.submeshIndex);
		}
		return label;
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
			if (collider.convexPlanes.empty() && collider.convexParts.empty())
			{
				return Physics::GetSphereAABBSweep(
					position, radius, movement,
					collider.minBounds, collider.maxBounds);
			}
			std::vector<const std::vector<Physics::ConvexPlane>*> planeSets;
			if (!collider.convexParts.empty())
			{
				for (const auto& part : collider.convexParts)
					planeSets.push_back(&part);
			}
			else
			{
				planeSets.push_back(&collider.convexPlanes);
			}
			Physics::SweepCollision earliestHit;
			for (const auto* planeSet : planeSets)
			{
				std::vector<Physics::ConvexPlane> expandedPlanes = *planeSet;
				for (Physics::ConvexPlane& plane : expandedPlanes)
					plane.distance += radius;
				const Physics::SweepCollision hit = Physics::GetConvexSweep(
					expandedPlanes, position, movement);
				if (hit.hit && (!earliestHit.hit || hit.time < earliestHit.time))
					earliestHit = hit;
			}
			return earliestHit;
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
			if (collider.convexPlanes.empty() && collider.convexParts.empty())
			{
				return Physics::SphereAABBOverlap(
					position, radius, collider.minBounds, collider.maxBounds);
			}
			std::vector<const std::vector<Physics::ConvexPlane>*> planeSets;
			if (!collider.convexParts.empty())
			{
				for (const auto& part : collider.convexParts)
					planeSets.push_back(&part);
			}
			else
			{
				planeSets.push_back(&collider.convexPlanes);
			}
			for (const auto* planeSet : planeSets)
			{
				bool overlapsPart = true;
				for (const Physics::ConvexPlane& plane : *planeSet)
				{
					if (glm::dot(plane.normal, position) > plane.distance + radius + 1e-5f)
					{
						overlapsPart = false;
						break;
					}
				}
				if (overlapsPart)
					return true;
			}
			return false;
		}
		case PhysicsColliderShape::Box:
		default:
			return Physics::SphereAABBOverlap(
				position, radius, collider.minBounds, collider.maxBounds);
		}
	}

	bool AABBOverlapsConvex(
		const glm::vec3& boxMin, const glm::vec3& boxMax,
		const PhysicsCollider& collider)
	{
		if (collider.convexPlanes.empty() && collider.convexParts.empty())
			return Physics::AABBOverlap(boxMin, boxMax, collider.minBounds, collider.maxBounds);

		// A plane separates the AABB from the convex collider when even the
		// AABB corner closest to the plane's interior lies beyond that plane.
		std::vector<const std::vector<Physics::ConvexPlane>*> planeSets;
		if (!collider.convexParts.empty())
		{
			for (const auto& part : collider.convexParts)
				planeSets.push_back(&part);
		}
		else
		{
			planeSets.push_back(&collider.convexPlanes);
		}
		for (const auto* planeSet : planeSets)
		{
			bool overlapsPart = true;
			for (const Physics::ConvexPlane& plane : *planeSet)
			{
				const glm::vec3 closestToInterior(
					plane.normal.x >= 0.0f ? boxMin.x : boxMax.x,
					plane.normal.y >= 0.0f ? boxMin.y : boxMax.y,
					plane.normal.z >= 0.0f ? boxMin.z : boxMax.z);
				if (glm::dot(plane.normal, closestToInterior) > plane.distance + 1e-5f)
				{
					overlapsPart = false;
					break;
				}
			}
			if (overlapsPart)
				return true;
		}
		return false;
	}

}

PhysicsWorld& PhysicsWorld::Instance()
{
	static PhysicsWorld world;
	return world;
}

bool PhysicsWorld::FindConvexSurfacePoint(
	const Entity& entity, const glm::vec2& horizontal, glm::vec3& point) const
{
	float highest = -std::numeric_limits<float>::max();
	bool found = false;
	Entity* object = const_cast<Entity*>(&entity);
	const Mesh* mesh = object->GetMesh();
	if (!mesh)
		return false;

	const auto& vertices = mesh->Vertices();
	const auto& faces = mesh->Faces();
	const glm::mat4 model = object->BuildModelMatrix();
	const bool baseMeshCollider = entity.UseBaseMeshCollider();
	const int firstSubmesh = baseMeshCollider ? -1 : 0;
	const int submeshLimit = baseMeshCollider ? 0 : mesh->NumBuffers();
	for (int submeshIndex = firstSubmesh; submeshIndex < submeshLimit; ++submeshIndex)
	{
		const SubmeshColliderSettings settings = baseMeshCollider
			? SubmeshColliderSettings{ true, entity.GetPhysicsColliderShape(), true }
			: entity.GetSubmeshColliderSettings(static_cast<std::size_t>(submeshIndex));
		if (!settings.enabled || settings.shape != PhysicsColliderShape::Convex)
			continue;

		PhysicsCollider collider;
		collider.owner = object;
		collider.submeshIndex = submeshIndex;
		collider.isBaseMeshCollider = baseMeshCollider;
		const std::vector<Physics::ConvexPlane> planes = BuildConvexPlanes(collider, submeshIndex);

		// Prefer authored triangle surfaces to preserve ramps and staircase treads.
		const std::size_t firstFace = baseMeshCollider ? 0
			: std::min<std::size_t>(mesh->FacesOffset(submeshIndex), faces.size());
		const std::size_t lastFace = baseMeshCollider ? faces.size()
			: std::min<std::size_t>(firstFace + mesh->FacesSize(submeshIndex), faces.size());
		for (std::size_t face = firstFace; face + 2 < lastFace; face += 3)
		{
			if (faces[face] >= vertices.size() || faces[face + 1] >= vertices.size() ||
				faces[face + 2] >= vertices.size())
				continue;

			const glm::vec3 a = glm::vec3(model * glm::vec4(vertices[faces[face]].position, 1.0f));
			const glm::vec3 b = glm::vec3(model * glm::vec4(vertices[faces[face + 1]].position, 1.0f));
			const glm::vec3 c = glm::vec3(model * glm::vec4(vertices[faces[face + 2]].position, 1.0f));
			const glm::vec3 normal = glm::cross(b - a, c - a);
			if (std::abs(normal.y) <= 1e-5f)
				continue;

			const glm::vec2 edge0(b.x - a.x, b.z - a.z);
			const glm::vec2 edge1(c.x - a.x, c.z - a.z);
			const glm::vec2 offset(horizontal.x - a.x, horizontal.y - a.z);
			const float denominator = edge0.x * edge1.y - edge1.x * edge0.y;
			if (std::abs(denominator) <= 1e-6f)
				continue;

			const float first = (offset.x * edge1.y - edge1.x * offset.y) / denominator;
			const float second = (edge0.x * offset.y - offset.x * edge0.y) / denominator;
			const float third = 1.0f - first - second;
			if (first < -1e-4f || second < -1e-4f || third < -1e-4f)
				continue;

			const float height = a.y * third + b.y * first + c.y * second;
			if (!found || height > highest)
			{
				highest = height;
				point = glm::vec3(horizontal.x, height, horizontal.y);
				found = true;
			}
		}

		for (const Physics::ConvexPlane& plane : planes)
		{
			// A downward rain ray can meet only an upward-facing boundary plane.
			if (plane.normal.y <= 1e-5f)
				continue;

			const float height = (plane.distance -
				plane.normal.x * horizontal.x - plane.normal.z * horizontal.y) /
				plane.normal.y;
			const glm::vec3 candidate(horizontal.x, height, horizontal.y);
			bool inside = true;
			for (const Physics::ConvexPlane& boundary : planes)
			{
				if (glm::dot(boundary.normal, candidate) > boundary.distance + 1e-3f)
				{
					inside = false;
					break;
				}
			}

			if (inside && (!found || height > highest))
			{
				highest = height;
				point = candidate;
				found = true;
			}
		}
	}

	// If the mesh does not provide a surface at this coordinate, use the
	// mathematical convex hull as a fallback.
	return found;
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
	for (const auto& collider : scene.LevelColliders())
	{
		if (collider)
		{
			Add(*collider);
		}
	}
}

ColliderHandle PhysicsWorld::Add(LevelCollider& collider)
{
	PhysicsCollider record;
	record.levelOwner = &collider;
	record.isStatic = true;
	record.enabled = true;
	record.shape = collider.Shape() == LevelColliderShape::Capsule
		? PhysicsColliderShape::Capsule : PhysicsColliderShape::Box;

	const glm::vec3 center = collider.Position();
	glm::vec3 dimensions = glm::abs(collider.Scale()) * 100.0f;
	if (collider.Shape() == LevelColliderShape::Plane)
	{
		// A plane is represented by a shallow slab extending below its authored
		// surface. Keeping the top face exactly at Position().y guarantees that
		// grounded sweeps receive an upward normal instead of selecting the
		// slab's bottom face when the player is resting on it.
		dimensions.y = 2.0f;
	}
	if (collider.Shape() == LevelColliderShape::Sphere)
	{
		const glm::vec3 absoluteScale = glm::abs(collider.Scale());
		const float radius = collider.Radius() * std::max(absoluteScale.x, std::max(absoluteScale.y, absoluteScale.z));
		record.minBounds = center - glm::vec3(radius);
		record.maxBounds = center + glm::vec3(radius);
	}
	else if (collider.Shape() == LevelColliderShape::Capsule)
	{
		const glm::vec3 absoluteScale = glm::abs(collider.Scale());
		record.capsuleRadii = glm::max(glm::vec3(collider.Radius()) * absoluteScale, glm::vec3(0.001f));
		record.capsuleRadius = glm::max(record.capsuleRadii.x, record.capsuleRadii.z);
		const float authoredHalfSegment = glm::max(0.0f, collider.Height() * 0.5f - collider.Radius());
		record.capsuleHalfLength = authoredHalfSegment * absoluteScale.y;
		glm::mat4 rotation(1.0f);
		rotation = glm::rotate(rotation, collider.Rotation().z, glm::vec3(0.0f, 0.0f, 1.0f));
		rotation = glm::rotate(rotation, collider.Rotation().y, glm::vec3(0.0f, 1.0f, 0.0f));
		rotation = glm::rotate(rotation, collider.Rotation().x, glm::vec3(1.0f, 0.0f, 0.0f));
		record.capsuleAxis = glm::normalize(glm::vec3(rotation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
		const glm::mat3 rotation3(rotation);
		const glm::vec3 rotatedCapExtents =
			glm::abs(rotation3[0]) * record.capsuleRadii.x +
			glm::abs(rotation3[1]) * record.capsuleRadii.y +
			glm::abs(rotation3[2]) * record.capsuleRadii.z;
		const glm::vec3 capsuleExtents = glm::abs(record.capsuleAxis) * record.capsuleHalfLength + rotatedCapExtents;
		record.minBounds = center - capsuleExtents;
		record.maxBounds = center + capsuleExtents;
	}
	else
	{
		const glm::vec3 halfExtents = dimensions * 0.5f;
		if (collider.Shape() == LevelColliderShape::Plane)
		{
			record.minBounds = center - glm::vec3(halfExtents.x, dimensions.y, halfExtents.z);
			record.maxBounds = center + glm::vec3(halfExtents.x, 0.0f, halfExtents.z);
		}
		else
		{
			record.minBounds = center - halfExtents;
			record.maxBounds = center + halfExtents;
		}
	}
	m_colliders.push_back(record);
	return m_colliders.size() - 1;
}

void PhysicsWorld::Update(LevelCollider& collider)
{
	for (ColliderHandle handle = 0; handle < m_colliders.size(); ++handle)
	{
		if (m_colliders[handle].levelOwner != &collider)
			continue;

		// Reuse the exact shape/bounds construction used during registration while
		// preserving the existing handle referenced by scene queries.
		const ColliderHandle temporaryHandle = Add(collider);
		m_colliders[handle] = m_colliders[temporaryHandle];
		m_colliders.pop_back();
		return;
	}
}

void PhysicsWorld::Remove(LevelCollider& collider)
{
	for (PhysicsCollider& record : m_colliders)
	{
		if (record.levelOwner == &collider)
		{
			record.enabled = false;
			return;
		}
	}
}

ColliderHandle PhysicsWorld::Find(const Entity& entity) const
{
	for (ColliderHandle handle = 0; handle < m_colliders.size(); ++handle)
	{
		const PhysicsCollider& collider = m_colliders[handle];
		if (collider.enabled && !collider.isHitbox && collider.owner == &entity)
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
	Entity** hitEntity,
	const PhysicsCollider** hitCollider) const
{
	FrameProfiler::Scope sweepScope(Root::Current().Profiler(), "Physics Sweep");
	// This method discovers a contact; the caller remains responsible for
	// applying movement and resolving the resulting slide.
	if (hitEntity)
	{
		*hitEntity = nullptr;
	}
	if (hitCollider)
	{
		*hitCollider = nullptr;
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
		if (!candidate.enabled || candidate.isHitbox || (!candidate.owner && !candidate.levelOwner))
		{
			continue;
		}
		if (candidate.owner && !candidate.owner->BlocksCollision())
		{
			continue;
		}
		if (candidate.owner && candidate.owner == moving.owner)
		{
			continue;
		}
		// Level planes are one-sided floor surfaces. Do not let a rising player
		// collide with the underside of the floor slab and cancel the jump.
		if (candidate.levelOwner && candidate.levelOwner->Shape() == LevelColliderShape::Plane && movement.y > 0.0f)
			continue;

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
			if (candidate.convexPlanes.empty() && candidate.convexParts.empty())
			{
				hit = useCapsule
					? Physics::GetCapsuleAABBSweep(
						capsuleBase, capsuleTip, moving.capsuleRadius, movement,
						candidate.minBounds, candidate.maxBounds)
					: Physics::GetAABBSweep(
						minBounds, maxBounds, movement,
						candidate.minBounds, candidate.maxBounds);
			}
			else
			{

				// The convex sweep receives a point moving through expanded planes.
				// The point is the moving shape's center, while its support distance
				// accounts for the moving shape's size.
				const glm::vec3 movingCenter = useCapsule
					? (capsuleBase + capsuleTip) * 0.5f
					: (minBounds + maxBounds) * 0.5f;
				const glm::vec3 movingHalfExtents = (maxBounds - minBounds) * 0.5f;
				const float capsuleHalfLength = useCapsule ? moving.capsuleHalfLength : 0.0f;

				// Expand every plane by the moving shape's support distance. Capsules
				// use radius plus axial length; boxes use projected half-extents.
				std::vector<const std::vector<Physics::ConvexPlane>*> planeSets;
				if (!candidate.convexParts.empty())
				{
					for (const auto& part : candidate.convexParts)
						planeSets.push_back(&part);
				}
				else
				{
					planeSets.push_back(&candidate.convexPlanes);
				}
				for (const auto* planeSet : planeSets)
				{
					std::vector<Physics::ConvexPlane> expandedPlanes = *planeSet;
					for (Physics::ConvexPlane& plane : expandedPlanes)
					{
						const float support = useCapsule
							? moving.capsuleRadius + capsuleHalfLength * std::abs(plane.normal.y)
							: glm::dot(glm::abs(plane.normal), movingHalfExtents);
						plane.distance += support;
					}
					const Physics::SweepCollision partHit = Physics::GetConvexSweep(
						expandedPlanes, movingCenter, movement);
					if (partHit.hit && (!hit.hit || partHit.time < hit.time))
						hit = partHit;
				}
			}
			const std::string movingName = moving.owner ? moving.owner->Name() : "<no owner>";
			const std::string candidateName = ConvexColliderLabel(candidate);
			Root::Current().Debugger().LogConvexCollision(
				"sweep|" + movingName + "|" + candidateName,
				"Sweep moving=" + movingName + " target=" + candidateName
				+ " result=" + (hit.hit ? "hit" : "miss")
				+ " planes=" + std::to_string(candidate.convexPlanes.size())
				+ " parts=" + std::to_string(candidate.convexParts.size())
				+ " fallback=" + (candidate.convexPlanes.empty() && candidate.convexParts.empty() ? "AABB" : "none")
				+ " planeSource=" + (candidate.convexParts.size() > 1 ? "thin-plane-compound"
					: candidate.convexFromThinPlane ? "thin-plane-hull"
					: candidate.convexPlanes.empty() ? "unavailable" : "mesh-faces")
				+ " time=" + std::to_string(hit.time)
				+ " normal=(" + std::to_string(hit.normal.x) + ","
				+ std::to_string(hit.normal.y) + "," + std::to_string(hit.normal.z) + ")"
				+ " movement=(" + std::to_string(movement.x) + ","
				+ std::to_string(movement.y) + "," + std::to_string(movement.z) + ")");
		}
		else if (useCapsule && candidate.shape == PhysicsColliderShape::Capsule)
		{
			const glm::vec3 candidateCenter = (candidate.minBounds + candidate.maxBounds) * 0.5f;
			const glm::vec3 candidateBase = candidateCenter - candidate.capsuleAxis * candidate.capsuleHalfLength;
			const glm::vec3 candidateTip = candidateCenter + candidate.capsuleAxis * candidate.capsuleHalfLength;
			hit = SweepVerticalCapsules(capsuleBase, capsuleTip, moving.capsuleRadius,
				movement, candidateBase, candidateTip,
				candidate.levelOwner ? candidate.capsuleRadii : glm::vec3(candidate.capsuleRadius));
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
		if (hit.hit && candidate.levelOwner && candidate.levelOwner->Shape() == LevelColliderShape::Plane)
		{
			// A plane only has a walkable top surface; never report its underside as
			// a collision normal while the controller is descending.
			hit.normal = glm::vec3(0.0f, 1.0f, 0.0f);
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
			if (hitCollider)
			{
				*hitCollider = &candidate;
			}
		}
	}

	return earliestHit;
}

Physics::SweepCollision PhysicsWorld::SweepEntity(
	const Entity& movingEntity, const glm::vec3& movement,
	const glm::vec3& offset, Entity** hitEntity,
	const PhysicsCollider** hitCollider) const
{
	if (hitEntity)
		*hitEntity = nullptr;
	if (hitCollider)
		*hitCollider = nullptr;
	Physics::SweepCollision earliestHit;
	for (ColliderHandle handle = 0; handle < m_colliders.size(); ++handle)
	{
		const PhysicsCollider& moving = m_colliders[handle];
		if (!moving.enabled || moving.isHitbox || moving.owner != &movingEntity)
			continue;

		Entity* candidateEntity = nullptr;
		const PhysicsCollider* candidateCollider = nullptr;
		const Physics::SweepCollision hit = Sweep(
			handle, moving.minBounds + offset, moving.maxBounds + offset,
			movement, &candidateEntity, &candidateCollider);
		if (hit.hit && hit.time < earliestHit.time)
		{
			earliestHit = hit;
			if (hitEntity)
				*hitEntity = candidateEntity;
			if (hitCollider)
				*hitCollider = candidateCollider;
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
	Physics::SweepCollision earliestHit;
	for (const PhysicsCollider& collider : m_colliders)
	{
		if (!collider.enabled || collider.isHitbox || collider.owner != &entity)
			continue;
		const Physics::SweepCollision hit = SweepCameraAgainstCollider(
			position, radius, movement, collider);
		if (hit.hit && hit.time < earliestHit.time)
			earliestHit = hit;
	}
	return earliestHit;
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

std::vector<Entity*> PhysicsWorld::QuerySphere(const glm::vec3& center, float radius, const Entity* ignoredEntity) const
{
	// Return unique non-owning entity pointers for owners with an enabled collider
	// overlapping the query sphere. The caller must not retain these pointers
	// beyond the owning scene's lifetime.
	std::vector<Entity*> result;
	const float safeRadius = std::max(radius, 0.0f);

	for (const PhysicsCollider& collider : m_colliders)
	{
		// Disabled, ownerless, and explicitly ignored colliders cannot enter a
		// trigger query.
		if (!collider.enabled || collider.isHitbox || !collider.owner || collider.owner == ignoredEntity)
			continue;
		// Reject distant colliders before invoking shape-specific tests. This is
		// especially important for convex meshes, whose exact test checks planes.
		if (!Physics::SphereAABBOverlap(
			center, safeRadius, collider.minBounds, collider.maxBounds))
		{
			continue;
		}

		// Use exact sphere tests for spherical colliders and shape-aware overlap
		// tests for boxes, capsules, and convex meshes.
		bool overlaps = false;
		if (collider.shape == PhysicsColliderShape::Sphere && collider.sphereRadius > 0.0f)
		{
			const glm::vec3 colliderCenter = (collider.minBounds + collider.maxBounds) * 0.5f;
			const float combinedRadius = safeRadius + collider.sphereRadius;
			overlaps = glm::dot(center - colliderCenter, center - colliderCenter)
				<= combinedRadius * combinedRadius;
		}
		else
		{
			overlaps = CameraOverlapsCollider(center, safeRadius, collider);
		}
		if (collider.shape == PhysicsColliderShape::Convex)
		{
			const std::string sourceName = ignoredEntity ? ignoredEntity->Name() : "<sphere-query>";
			const std::string candidateName = ConvexColliderLabel(collider);
			Root::Current().Debugger().LogConvexCollision(
				"spherequery|" + sourceName + "|" + candidateName,
				"Sphere query source=" + sourceName + " target=" + candidateName
				+ " result=" + (overlaps ? "overlap" : "separate")
				+ " planes=" + std::to_string(collider.convexPlanes.size())
				+ " parts=" + std::to_string(collider.convexParts.size())
				+ " fallback=" + (collider.convexPlanes.empty() && collider.convexParts.empty() ? "AABB" : "none")
				+ " center=(" + std::to_string(center.x) + ","
				+ std::to_string(center.y) + "," + std::to_string(center.z) + ")"
				+ " radius=" + std::to_string(safeRadius));
		}

		if (overlaps)
		{
			// PhysicsWorld does not own entities; only report the collider owner.
			if (std::find(result.begin(), result.end(), collider.owner) == result.end())
				result.push_back(collider.owner);
		}
	}

	return result;
}

std::vector<Entity*> PhysicsWorld::QueryHitbox(ColliderHandle hitboxHandle, const Entity* ignoredEntity) const
{
	std::vector<Entity*> result;
	if (hitboxHandle >= m_colliders.size())
		return result;

	const PhysicsCollider& hitbox = m_colliders[hitboxHandle];
	if (!hitbox.enabled || !hitbox.isHitbox || !hitbox.owner)
		return result;

	const glm::vec3 hitboxCenter = (hitbox.minBounds + hitbox.maxBounds) * 0.5f;
	for (const PhysicsCollider& candidate : m_colliders)
	{
		if (!candidate.enabled || candidate.isHitbox || !candidate.owner
			|| candidate.owner == hitbox.owner || candidate.owner == ignoredEntity)
			continue;

		bool overlaps = false;
		if (hitbox.shape == PhysicsColliderShape::Sphere && hitbox.sphereRadius > 0.0f)
		{
			if (candidate.shape == PhysicsColliderShape::Sphere && candidate.sphereRadius > 0.0f)
			{
				const glm::vec3 candidateCenter = (candidate.minBounds + candidate.maxBounds) * 0.5f;
				const float combinedRadius = hitbox.sphereRadius + candidate.sphereRadius;
				overlaps = glm::dot(hitboxCenter - candidateCenter, hitboxCenter - candidateCenter)
					<= combinedRadius * combinedRadius;
			}
			else if (candidate.shape == PhysicsColliderShape::Capsule && candidate.capsuleRadius > 0.0f)
			{
				const glm::vec3 candidateCenter = (candidate.minBounds + candidate.maxBounds) * 0.5f;
				const glm::vec3 halfAxis = candidate.capsuleAxis * candidate.capsuleHalfLength;
				overlaps = Physics::SphereCapsuleOverlap(hitboxCenter, hitbox.sphereRadius,
					candidateCenter - halfAxis, candidateCenter + halfAxis, candidate.capsuleRadius);
			}
			else
			{
				overlaps = CameraOverlapsCollider(hitboxCenter, hitbox.sphereRadius, candidate);
			}
		}
		else
		{
			if (candidate.shape == PhysicsColliderShape::Convex)
				overlaps = AABBOverlapsConvex(
					hitbox.minBounds, hitbox.maxBounds, candidate);
			else
				overlaps = Physics::AABBOverlap(hitbox.minBounds, hitbox.maxBounds,
					candidate.minBounds, candidate.maxBounds);
		}

		if (overlaps && std::find(result.begin(), result.end(), candidate.owner) == result.end())
			result.push_back(candidate.owner);
		if (candidate.shape == PhysicsColliderShape::Convex)
		{
			const std::string hitboxName = hitbox.owner->Name();
			const std::string candidateName = ConvexColliderLabel(candidate);
			Root::Current().Debugger().LogConvexCollision(
				"hitbox|" + hitboxName + "|" + candidateName,
				"Box hitbox query source=" + hitboxName + " target=" + candidateName
				+ " result=" + (overlaps ? "overlap" : "separate")
				+ " planes=" + std::to_string(candidate.convexPlanes.size())
				+ " parts=" + std::to_string(candidate.convexParts.size())
				+ " fallback=" + (candidate.convexPlanes.empty() && candidate.convexParts.empty() ? "AABB" : "none"));
		}
	}

	return result;
}

PhysicsCollider PhysicsWorld::BuildSubmeshCollider(
	const Entity& entity, int submeshIndex) const
{
	const auto buildStart = std::chrono::steady_clock::now();
	PhysicsCollider collider;
	Entity& mutableEntity = const_cast<Entity&>(entity);
	Mesh* mesh = mutableEntity.GetMesh();
	const bool baseMeshCollider = submeshIndex < 0;
	if (!mesh || (baseMeshCollider
		? !entity.UseBaseMeshCollider()
		: submeshIndex >= std::max(mesh->NumBuffers(), 0)))
	{
		collider.enabled = false;
		return collider;
	}

	const SubmeshColliderSettings settings = baseMeshCollider
		? SubmeshColliderSettings{ true, entity.GetPhysicsColliderShape(), true }
		: entity.GetSubmeshColliderSettings(static_cast<std::size_t>(submeshIndex));
	collider.owner = const_cast<Entity*>(&entity);
	collider.submeshIndex = submeshIndex;
	collider.isBaseMeshCollider = baseMeshCollider;
	collider.enabled = settings.enabled;
	collider.shape = settings.shape;
	collider.isStatic = mutableEntity.GetController() == nullptr && !entity.IsRigidBody();
	if (!settings.enabled)
		return collider;

	// Derive the submesh's local bounds from its index range. This also handles
	// meshes created directly from vertices/faces, which have no imported bounds
	// array but still expose one render buffer.
	glm::vec3 localMin(std::numeric_limits<float>::max());
	glm::vec3 localMax(-std::numeric_limits<float>::max());
	bool hasLocalBounds = false;
	if (baseMeshCollider)
	{
		localMin = mesh->minBounds();
		localMax = mesh->maxBounds();
		hasLocalBounds = true;
	}
	else if (submeshIndex < mesh->NumBuffers())
	{
		localMin = mesh->SubMeshMinBounds(submeshIndex);
		localMax = mesh->SubMeshMaxBounds(submeshIndex);
		hasLocalBounds = true;
	}

	// Mesh bounds are cached at import time. Retain the face-index fallback for
	// meshes created procedurally or imported without valid per-submesh bounds.
	const auto& vertices = mesh->Vertices();
	const auto& faces = mesh->Faces();
	const std::size_t firstIndex = baseMeshCollider ? 0
		: std::min<std::size_t>(mesh->FacesOffset(submeshIndex), faces.size());
	const std::size_t lastIndex = baseMeshCollider ? faces.size()
		: std::min<std::size_t>(firstIndex + mesh->FacesSize(submeshIndex), faces.size());
	if (!hasLocalBounds || !std::isfinite(localMin.x) || !std::isfinite(localMin.y)
		|| !std::isfinite(localMin.z) || !std::isfinite(localMax.x)
		|| !std::isfinite(localMax.y) || !std::isfinite(localMax.z)
		|| localMin.x > localMax.x || localMin.y > localMax.y || localMin.z > localMax.z)
	{
		hasLocalBounds = false;
		for (std::size_t index = firstIndex; index < lastIndex; ++index)
		{
			const std::uint32_t vertexIndex = faces[index];
			if (vertexIndex >= vertices.size())
				continue;
			localMin = glm::min(localMin, vertices[vertexIndex].position);
			localMax = glm::max(localMax, vertices[vertexIndex].position);
			hasLocalBounds = true;
		}
	}
	if (!hasLocalBounds)
	{
		collider.enabled = false;
		return collider;
	}

	// Transform all eight corners so rotation, nonuniform scale, and parent
	// transforms produce a conservative world-space AABB for this submesh.
	const glm::mat4 model = mutableEntity.BuildModelMatrix();
	collider.cachedModelMatrix = model;
	collider.hasCachedModelMatrix = true;
	const glm::vec3 first = glm::vec3(model * glm::vec4(localMin, 1.0f));
	collider.minBounds = first;
	collider.maxBounds = first;
	for (int x = 0; x < 2; ++x)
	{
		for (int y = 0; y < 2; ++y)
		{
			for (int z = 0; z < 2; ++z)
			{
				const glm::vec3 corner(
					x == 0 ? localMin.x : localMax.x,
					y == 0 ? localMin.y : localMax.y,
					z == 0 ? localMin.z : localMax.z);
				const glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corner, 1.0f));
				collider.minBounds = glm::min(collider.minBounds, worldCorner);
				collider.maxBounds = glm::max(collider.maxBounds, worldCorner);
			}
		}
	}

	if (collider.shape == PhysicsColliderShape::Convex)
	{
		collider.convexPlanes = BuildConvexPlanes(
			collider, submeshIndex, &collider.convexFromThinPlane);
		if (collider.convexPlanes.empty())
		{
			collider.convexParts = BuildPlanarConvexParts(collider, submeshIndex);
			collider.convexFromThinPlane = !collider.convexParts.empty();
			if (collider.convexFromThinPlane)
			{
				// Keep broadphase and overlap queries aware of the slab thickness.
				collider.minBounds -= glm::vec3(1.0f);
				collider.maxBounds += glm::vec3(1.0f);
			}
		}
		const std::string colliderName = ConvexColliderLabel(collider);
		Root::Current().Debugger().LogConvexCollision(
			"build|" + colliderName,
			"Built " + colliderName + " planes=" + std::to_string(collider.convexPlanes.size())
			+ " parts=" + std::to_string(collider.convexParts.size())
			+ " fallback=" + (collider.convexPlanes.empty() && collider.convexParts.empty() ? "AABB" : "none")
			+ " planeSource=" + (collider.convexParts.size() > 1 ? "thin-plane-compound"
				: collider.convexFromThinPlane ? "thin-plane-hull"
				: collider.convexPlanes.empty() ? "unavailable" : "mesh-faces")
			+ " boundsMin=(" + std::to_string(collider.minBounds.x) + ","
			+ std::to_string(collider.minBounds.y) + "," + std::to_string(collider.minBounds.z) + ")"
			+ " boundsMax=(" + std::to_string(collider.maxBounds.x) + ","
			+ std::to_string(collider.maxBounds.y) + "," + std::to_string(collider.maxBounds.z) + ")");
	}
	else if (collider.shape == PhysicsColliderShape::Capsule)
	{
		// Derive a vertical capsule from local bounds and the model's scale axes.
		// Measuring the rotated world AABB makes radius and height change as the
		// character turns, which causes collision contacts to jump between frames.
		const glm::vec3 worldScale(
			glm::length(glm::vec3(model[0])),
			glm::length(glm::vec3(model[1])),
			glm::length(glm::vec3(model[2])));
		const glm::vec3 localHalfExtents = (localMax - localMin) * 0.5f;
		glm::vec3 capsuleCenter = glm::vec3(model * glm::vec4((localMin + localMax) * 0.5f, 1.0f));
		const float halfWidthX = localHalfExtents.x * worldScale.x;
		const float halfWidthZ = localHalfExtents.z * worldScale.z;
		float lowestVertexY = std::numeric_limits<float>::max();
		float highestVertexY = -std::numeric_limits<float>::max();
		auto includeVertexY = [&](std::uint32_t vertexIndex)
		{
			if (vertexIndex >= vertices.size())
				return;
			const float worldY = (model * glm::vec4(vertices[vertexIndex].position, 1.0f)).y;
			lowestVertexY = std::min(lowestVertexY, worldY);
			highestVertexY = std::max(highestVertexY, worldY);
		};
		if (baseMeshCollider)
		{
			for (std::size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex)
				includeVertexY(static_cast<std::uint32_t>(vertexIndex));
		}
		else
		{
			for (std::size_t faceIndex = firstIndex; faceIndex < lastIndex; ++faceIndex)
				includeVertexY(faces[faceIndex]);
		}
		if (!std::isfinite(lowestVertexY) || !std::isfinite(highestVertexY))
		{
			lowestVertexY = collider.minBounds.y;
			highestVertexY = collider.maxBounds.y;
		}
		const float halfHeight = (highestVertexY - lowestVertexY) * 0.5f;
		const float halfCapsuleHeight = glm::max(halfHeight, 0.001f);
		capsuleCenter.y = (lowestVertexY + highestVertexY) * 0.5f;
		const float meshDerivedRadius = glm::max(
			glm::min(glm::min(halfWidthX, halfWidthZ), halfHeight), 0.001f);
		collider.capsuleRadius = glm::min(entity.CapsuleRadiusOverride() > 0.0f
			? entity.CapsuleRadiusOverride() : meshDerivedRadius, halfCapsuleHeight);
		collider.capsuleHalfLength = glm::max(halfHeight - collider.capsuleRadius, 0.0f);
		collider.capsuleRadii = glm::vec3(collider.capsuleRadius);
		collider.capsuleAxis = glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 capsuleExtents(collider.capsuleRadius,
			collider.capsuleHalfLength + collider.capsuleRadius, collider.capsuleRadius);
		collider.minBounds = capsuleCenter - capsuleExtents;
		collider.maxBounds = capsuleCenter + capsuleExtents;
	}
	else if (collider.shape == PhysicsColliderShape::Sphere)
	{
		const glm::vec3 center = (collider.minBounds + collider.maxBounds) * 0.5f;
		collider.sphereRadius = glm::length((collider.maxBounds - collider.minBounds) * 0.5f);
		collider.minBounds = center - glm::vec3(collider.sphereRadius);
		collider.maxBounds = center + glm::vec3(collider.sphereRadius);
	}

	const double buildMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - buildStart).count();
	if (buildMs >= 0.5)
	{
		Root::Current().Debugger().LogTagged("Physics Sync",
			"slow collider build entity=\"" + entity.Name() + "\" submesh="
			+ std::to_string(submeshIndex) + " shape="
			+ std::to_string(static_cast<int>(collider.shape)) + " faces="
			+ std::to_string(mesh ? mesh->FacesSize(submeshIndex) : 0) + " vertices="
			+ std::to_string(mesh ? mesh->Vertices().size() : 0) + " convexPlanes="
			+ std::to_string(collider.convexPlanes.size()) + " convexParts="
			+ std::to_string(collider.convexParts.size()) + " elapsedMs="
			+ std::to_string(buildMs));
	}
	return collider;
}

ColliderHandle PhysicsWorld::Add(Entity& entity)
{
	Mesh* mesh = entity.GetMesh();
	if (!mesh)
		return InvalidColliderHandle;
	for (const PhysicsCollider& existing : m_colliders)
	{
		if (existing.owner == &entity && !existing.isHitbox)
		{
			// Add is idempotent. Callers that intentionally change transforms or
			// collider settings use Update(entity), avoiding an accidental expensive
			// convex rebuild when an already registered entity is added again.
			return Find(entity);
		}
	}

	entity.SyncSubmeshColliderSettings();
	if (entity.UseBaseMeshCollider())
	{
		PhysicsCollider collider = BuildSubmeshCollider(entity, -1);
		if (!collider.enabled)
			return InvalidColliderHandle;
		m_colliders.push_back(std::move(collider));
		return m_colliders.size() - 1;
	}
	const std::size_t submeshCount = entity.GetSubmeshColliderSettings().size();
	ColliderHandle firstHandle = InvalidColliderHandle;
	for (std::size_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
	{
		PhysicsCollider collider = BuildSubmeshCollider(entity, static_cast<int>(submeshIndex));
		if (!collider.enabled)
			continue;
		m_colliders.push_back(std::move(collider));
		if (firstHandle == InvalidColliderHandle)
			firstHandle = m_colliders.size() - 1;
	}
	return firstHandle;
}

ColliderHandle PhysicsWorld::AddHitbox(Entity& entity, PhysicsColliderShape shape,
	const glm::vec3& center, const glm::vec3& halfExtents)
{
	PhysicsCollider collider;
	collider.owner = &entity;
	collider.shape = shape;
	collider.minBounds = center - halfExtents;
	collider.maxBounds = center + halfExtents;
	collider.isStatic = false;
	collider.isHitbox = true;
	collider.sphereRadius = shape == PhysicsColliderShape::Sphere ? halfExtents.x : 0.0f;
	m_colliders.push_back(collider);
	return m_colliders.size() - 1;
}

void PhysicsWorld::RemoveHitbox(ColliderHandle handle, Entity& entity)
{
	if (handle >= m_colliders.size()) return;
	if (!m_colliders[handle].isHitbox || m_colliders[handle].owner != &entity) return;
	Remove(handle);
}

bool PhysicsWorld::UpdateHitbox(ColliderHandle handle, Entity& entity, PhysicsColliderShape shape,
	const glm::vec3& center, const glm::vec3& halfExtents)
{
	if (handle >= m_colliders.size()) return false;
	PhysicsCollider& collider = m_colliders[handle];
	if (!collider.isHitbox || collider.owner != &entity) return false;
	collider.shape = shape;
	collider.minBounds = center - halfExtents;
	collider.maxBounds = center + halfExtents;
	collider.sphereRadius = shape == PhysicsColliderShape::Sphere ? halfExtents.x : 0.0f;
	collider.enabled = true;
	return true;
}

void PhysicsWorld::Update(ColliderHandle handle)
{
	FrameProfiler::Scope syncScope(Root::Current().Profiler(), "Physics Sync");
	// Ignore stale handles instead of allowing an invalid index to access the
	// packed collider storage.
	if (handle >= m_colliders.size())
	{
		return;
	}

	PhysicsCollider& collider = m_colliders[handle];
	if (!collider.owner || collider.isHitbox
		|| (collider.submeshIndex < 0 && !collider.isBaseMeshCollider))
	{
		collider.enabled = false;
		return;
	}
	collider = BuildSubmeshCollider(*collider.owner, collider.submeshIndex);
}

void PhysicsWorld::Update(Entity& entity)
{
	const auto updateStart = std::chrono::steady_clock::now();
	entity.SyncSubmeshColliderSettings();
	Mesh* mesh = entity.GetMesh();
	const std::size_t submeshCount = mesh
		? entity.GetSubmeshColliderSettings().size() : 0;
	std::vector<bool> updatedSubmeshes(submeshCount, false);
	bool updatedBaseMesh = false;
	auto logSlowUpdate = [&]()
	{
		const double updateMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - updateStart).count();
		if (updateMs < 0.5)
			return;
		std::size_t ownedColliders = 0;
		for (const PhysicsCollider& collider : m_colliders)
			if (collider.owner == &entity && !collider.isHitbox)
				++ownedColliders;
		Root::Current().Debugger().LogTagged("Physics Sync",
			"slow entity update entity=\"" + entity.Name() + "\" colliders="
			+ std::to_string(ownedColliders) + " sceneColliders="
			+ std::to_string(m_colliders.size()) + " faces="
			+ std::to_string(mesh ? mesh->Faces().size() : 0) + " elapsedMs="
			+ std::to_string(updateMs));
	};

	for (ColliderHandle handle = 0; handle < m_colliders.size(); ++handle)
	{
		PhysicsCollider& existing = m_colliders[handle];
		if (existing.owner != &entity || existing.isHitbox)
			continue;

		if (existing.isBaseMeshCollider)
		{
			if (!entity.UseBaseMeshCollider() || updatedBaseMesh)
				existing = PhysicsCollider{};
			else
			{
				const glm::mat4 model = entity.BuildModelMatrix();
				const PhysicsColliderShape shape = entity.GetPhysicsColliderShape();
				const bool shouldBeEnabled = true;
				if (!existing.isStatic || !existing.hasCachedModelMatrix
					|| !SameMatrix(existing.cachedModelMatrix, model)
					|| existing.shape != shape || existing.enabled != shouldBeEnabled)
					existing = BuildSubmeshCollider(entity, -1);
				updatedBaseMesh = true;
			}
			continue;
		}

		if (entity.UseBaseMeshCollider() || existing.submeshIndex < 0
			|| static_cast<std::size_t>(existing.submeshIndex) >= submeshCount)
		{
			existing = PhysicsCollider{};
			continue;
		}

		const std::size_t submeshIndex = static_cast<std::size_t>(existing.submeshIndex);
		if (updatedSubmeshes[submeshIndex])
		{
			existing.enabled = false;
			continue;
		}
		const SubmeshColliderSettings settings = entity.GetSubmeshColliderSettings(submeshIndex);
		const glm::mat4 model = entity.BuildModelMatrix();
		if (!existing.isStatic || !existing.hasCachedModelMatrix
			|| !SameMatrix(existing.cachedModelMatrix, model)
			|| existing.shape != settings.shape || existing.enabled != settings.enabled)
			existing = BuildSubmeshCollider(entity, static_cast<int>(submeshIndex));
		updatedSubmeshes[submeshIndex] = true;
	}

	if (entity.UseBaseMeshCollider())
	{
		if (!updatedBaseMesh)
		{
			PhysicsCollider collider = BuildSubmeshCollider(entity, -1);
			if (collider.enabled)
				m_colliders.push_back(std::move(collider));
		}
		logSlowUpdate();
		return;
	}

	for (std::size_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
	{
		if (updatedSubmeshes[submeshIndex])
			continue;
		PhysicsCollider collider = BuildSubmeshCollider(entity, static_cast<int>(submeshIndex));
		if (collider.enabled)
			m_colliders.push_back(std::move(collider));
	}
	logSlowUpdate();
}

void PhysicsWorld::SimulateRigidBody(Entity& entity, float dt)
{
	if (dt <= 0.0f || !entity.IsRigidBody() || entity.GetController())
		return;

	const ColliderHandle collider = Find(entity);
	if (collider == InvalidColliderHandle)
		return;

	// Engine assets use centimeters as world units, matching Controller gravity.
	constexpr float gravity = 981.0f;
	constexpr float terminalFallSpeed = 5500.0f;
	constexpr float collisionSkin = 0.1f;
	constexpr int maxSlideIterations = 4;
	glm::vec3 velocity = entity.LinearVelocity();
	velocity.y = std::max(velocity.y - gravity * dt, -terminalFallSpeed);
	glm::vec3 remainingMovement = velocity * dt;
	glm::vec3 resolvedMovement(0.0f);

	for (int iteration = 0; iteration < maxSlideIterations; ++iteration)
	{
		if (glm::length(remainingMovement) <= 0.0001f)
			break;

		const Physics::SweepCollision collision = SweepEntity(
			entity, remainingMovement, resolvedMovement);
		if (!collision.hit)
		{
			resolvedMovement += remainingMovement;
			break;
		}

		const glm::vec3 normal = glm::normalize(collision.normal);
		const float hitTime = glm::clamp(collision.time, 0.0f, 1.0f);
		resolvedMovement += remainingMovement * hitTime;
		remainingMovement *= 1.0f - hitTime;

		const float velocityIntoSurface = glm::dot(velocity, normal);
		if (velocityIntoSurface < 0.0f)
			velocity -= normal * velocityIntoSurface;

		const float movementIntoSurface = glm::dot(remainingMovement, normal);
		if (movementIntoSurface < 0.0f)
			remainingMovement -= normal * movementIntoSurface;

		if (normal.y >= 0.65f && velocity.y <= 0.0f)
			velocity.y = 0.0f;
		resolvedMovement += normal * collisionSkin;
	}

	entity.Move(resolvedMovement);
	// Exponential damping is stable across frame rates. A small rest threshold
	// prevents residual motion from making a landed body slide forever.
	const float damping = std::max(0.0f, entity.LinearDamping());
	if (damping > 0.0f)
		velocity *= std::exp(-damping * dt);
	if (glm::length(velocity) < 5.0f)
		velocity = glm::vec3(0.0f);
	entity.SetLinearVelocity(velocity);
	Update(entity);
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

void PhysicsWorld::Remove(Entity& entity)
{
	for (PhysicsCollider& collider : m_colliders)
	{
		if (collider.owner == &entity)
		{
			collider = PhysicsCollider{};
			collider.enabled = false;
		}
	}
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
