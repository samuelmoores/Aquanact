#include "Engine/Core/Physics.h"
#include <algorithm>
#include <cmath>
#include <limits>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

bool Physics::AABBOverlap(
	const glm::vec3& minA, const glm::vec3& maxA,
	const glm::vec3& minB, const glm::vec3& maxB)
{
	return (minA.x <= maxB.x && maxA.x >= minB.x)
		&& (minA.y <= maxB.y && maxA.y >= minB.y)
		&& (minA.z <= maxB.z && maxA.z >= minB.z);
}

bool Physics::SweepAABB(
	const glm::vec3& minA, const glm::vec3& maxA,
	const glm::vec3& movement,
	const glm::vec3& minB, const glm::vec3& maxB)
{
	return GetAABBSweep(minA, maxA, movement, minB, maxB).hit;
}

Physics::SweepCollision Physics::GetAABBSweep(
	const glm::vec3& minA, const glm::vec3& maxA,
	const glm::vec3& movement,
	const glm::vec3& minB, const glm::vec3& maxB)
{
	SweepCollision result;
	const glm::vec3 center = (minA + maxA) * 0.5f;
	const glm::vec3 halfExtents = (maxA - minA) * 0.5f;
	const glm::vec3 expandedMin = minB - halfExtents;
	const glm::vec3 expandedMax = maxB + halfExtents;

	if (center.x >= expandedMin.x && center.x <= expandedMax.x &&
		center.y >= expandedMin.y && center.y <= expandedMax.y &&
		center.z >= expandedMin.z && center.z <= expandedMax.z)
	{
		const float distances[6] = {
			center.x - expandedMin.x, expandedMax.x - center.x,
			center.y - expandedMin.y, expandedMax.y - center.y,
			center.z - expandedMin.z, expandedMax.z - center.z
		};
		const glm::vec3 normals[6] = {
			{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
			{0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}
		};
		int closestFace = 0;
		for (int i = 1; i < 6; ++i)
		{
			if (distances[i] < distances[closestFace])
			{
				closestFace = i;
			}
		}

		if (glm::dot(movement, normals[closestFace]) <= 0.0f)
		{
			result.hit = true;
			result.normal = normals[closestFace];
			result.time = 0.0f;
		}
		return result;
	}

	float enterTime = 0.0f;
	float exitTime = 1.0f;
	glm::vec3 enterNormal(0.0f);
	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::abs(movement[axis]) <= 1e-6f)
		{
			if (center[axis] < expandedMin[axis] || center[axis] > expandedMax[axis])
			{
				return result;
			}
			continue;
		}

		float nearTime = (expandedMin[axis] - center[axis]) / movement[axis];
		float farTime = (expandedMax[axis] - center[axis]) / movement[axis];
		glm::vec3 nearNormal(0.0f);
		nearNormal[axis] = movement[axis] > 0.0f ? -1.0f : 1.0f;
		if (nearTime > farTime)
		{
			std::swap(nearTime, farTime);
		}

		if (nearTime >= enterTime)
		{
			enterTime = nearTime;
			enterNormal = nearNormal;
		}
		exitTime = std::min(exitTime, farTime);
		if (enterTime > exitTime)
		{
			return result;
		}
	}

	if (enterTime < 0.0f || enterTime > 1.0f || glm::length2(enterNormal) <= 0.0f)
	{
		return result;
	}

	result.hit = true;
	result.normal = enterNormal;
	result.time = enterTime;
	return result;
}

bool Physics::SphereAABBOverlap(
	const glm::vec3& center, float radius,
	const glm::vec3& boxMin, const glm::vec3& boxMax)
{
	const glm::vec3 closest = glm::clamp(center, boxMin, boxMax);
	return glm::dot(center - closest, center - closest) <= radius * radius;
}

bool Physics::SweepSphereAABB(
	const glm::vec3& center, float radius,
	const glm::vec3& movement,
	const glm::vec3& boxMin, const glm::vec3& boxMax)
{
	return GetSphereAABBSweep(center, radius, movement, boxMin, boxMax).hit;
}

Physics::SweepCollision Physics::GetSphereAABBSweep(
	const glm::vec3& center, float radius,
	const glm::vec3& movement,
	const glm::vec3& boxMin, const glm::vec3& boxMax)
{
	SweepCollision result;
	const glm::vec3 expandedMin = boxMin - glm::vec3(radius);
	const glm::vec3 expandedMax = boxMax + glm::vec3(radius);
	float enterTime = 0.0f;
	float exitTime = 1.0f;
	glm::vec3 enterNormal(0.0f);

	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::abs(movement[axis]) <= 1e-6f)
		{
			if (center[axis] < expandedMin[axis] || center[axis] > expandedMax[axis])
			{
				return result;
			}
			continue;
		}

		float nearTime = (expandedMin[axis] - center[axis]) / movement[axis];
		float farTime = (expandedMax[axis] - center[axis]) / movement[axis];
		glm::vec3 nearNormal(0.0f);
		nearNormal[axis] = movement[axis] > 0.0f ? -1.0f : 1.0f;
		if (nearTime > farTime)
		{
			std::swap(nearTime, farTime);
		}

		if (nearTime >= enterTime)
		{
			enterTime = nearTime;
			enterNormal = nearNormal;
		}
		exitTime = std::min(exitTime, farTime);
		if (enterTime > exitTime)
		{
			return result;
		}
	}

	if (enterTime < 0.0f || enterTime > 1.0f || glm::length2(enterNormal) <= 0.0f)
	{
		return result;
	}

	result.hit = true;
	result.normal = enterNormal;
	result.time = enterTime;
	return result;
}

Physics::Collision Physics::GetSphereAABBCollision(
	const glm::vec3& center, float radius,
	const glm::vec3& boxMin, const glm::vec3& boxMax,
	const glm::vec3& movement)
{
	Collision result = { false, glm::vec3(0.0f), 0.0f };
	const glm::vec3 sphereCenter = center + movement;
	const glm::vec3 closest = glm::clamp(sphereCenter, boxMin, boxMax);
	const glm::vec3 offset = sphereCenter - closest;
	const float distanceSquared = glm::dot(offset, offset);
	if (distanceSquared > radius * radius)
	{
		return result;
	}

	result.hit = true;
	const float distance = std::sqrt(distanceSquared);
	if (distance > 1e-6f)
	{
		result.normal = offset / distance;
		result.penetration = radius - distance;
		return result;
	}

	const float distances[6] = {
		sphereCenter.x - boxMin.x, boxMax.x - sphereCenter.x,
		sphereCenter.y - boxMin.y, boxMax.y - sphereCenter.y,
		sphereCenter.z - boxMin.z, boxMax.z - sphereCenter.z
	};
	const glm::vec3 normals[6] = {
		{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
		{0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}
	};
	int closestFace = 0;
	for (int i = 1; i < 6; ++i)
	{
		const float distanceDifference = distances[i] - distances[closestFace];
		const bool closer = distanceDifference < -1e-5f;
		const bool tiedAndOpposesMovement = std::abs(distanceDifference) <= 1e-5f
			&& glm::dot(normals[i], movement) < glm::dot(normals[closestFace], movement);
		if (closer || tiedAndOpposesMovement)
		{
			closestFace = i;
		}
	}
	result.normal = normals[closestFace];
	result.penetration = radius + distances[closestFace];
	return result;
}

static glm::vec3 ClosestPointOnSegment(const glm::vec3& a, const glm::vec3& b, const glm::vec3& p)
{
	glm::vec3 ab = b - a;
	float lenSq = glm::dot(ab, ab);
	if (lenSq < 1e-10f) return a;
	float t = glm::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
	return a + t * ab;
}

bool Physics::CapsuleAABBOverlap(
	const glm::vec3& capBase, const glm::vec3& capTip, float radius,
	const glm::vec3& boxMin, const glm::vec3& boxMax)
{
	// Two-pass: project box center onto spine, clamp to box, re-project onto spine
	glm::vec3 boxCenter = (boxMin + boxMax) * 0.5f;
	glm::vec3 spinePoint = ClosestPointOnSegment(capBase, capTip, boxCenter);
	glm::vec3 boxClosest = glm::clamp(spinePoint, boxMin, boxMax);
	spinePoint = ClosestPointOnSegment(capBase, capTip, boxClosest);
	return glm::length2(spinePoint - boxClosest) <= radius * radius;
}

bool Physics::SweepCapsuleAABB(
	const glm::vec3& capBase, const glm::vec3& capTip, float radius,
	const glm::vec3& movement,
	const glm::vec3& boxMin, const glm::vec3& boxMax)
{
	return CapsuleAABBOverlap(capBase + movement, capTip + movement, radius, boxMin, boxMax);
}

Physics::Collision Physics::GetCapsuleAABBCollision(
	const glm::vec3& capBase, const glm::vec3& capTip, float radius,
	const glm::vec3& boxMin, const glm::vec3& boxMax,
	const glm::vec3& movement)
{
	Collision result = { false, glm::vec3(0.0f), 0.0f };

	glm::vec3 boxCenter = (boxMin + boxMax) * 0.5f;
	glm::vec3 spinePoint = ClosestPointOnSegment(capBase, capTip, boxCenter);
	glm::vec3 boxClosest = glm::clamp(spinePoint, boxMin, boxMax);
	spinePoint = ClosestPointOnSegment(capBase, capTip, boxClosest);

	glm::vec3 v = spinePoint - boxClosest;
	float distSq = glm::dot(v, v);

	if (distSq > radius * radius) return result;

	float dist = sqrtf(distSq);
	result.hit = true;
	if (dist > 1e-6f)
	{
		result.normal = v / dist;
		result.penetration = radius - dist;
	}
	else
	{
		// Spine point is exactly on or inside the box surface.
		// We need to pick the best face to push out of.
		// We prioritize faces that are most "against" the movement direction.
		glm::vec3 dMin = spinePoint - boxMin;
		glm::vec3 dMax = boxMax - spinePoint;

		struct Face { glm::vec3 normal; float dist; };
		Face faces[6] = {
			{{-1, 0, 0}, dMin.x}, {{1, 0, 0}, dMax.x},
			{{0, -1, 0}, dMin.y}, {{0, 1, 0}, dMax.y},
			{{0, 0, -1}, dMin.z}, {{0, 0, 1}, dMax.z}
		};

		int bestFace = 0;
		float bestScore = -1e10f;

		for (int i = 0; i < 6; i++)
		{
			// Score is based on:
			// 1. How much it opposes movement (dot(normal, movement) < 0 is good)
			// 2. Proximity (smaller distance is better for depenetration)
			float moveOppose = -glm::dot(faces[i].normal, movement);
			
			// We want a face that either opposes movement OR is the absolute closest.
			// This heuristic helps prevent "popping" through the back of thin walls.
			float score = moveOppose * 10.0f - faces[i].dist;
			if (score > bestScore)
			{
				bestScore = score;
				bestFace = i;
			}
		}

		result.normal = faces[bestFace].normal;
		result.penetration = radius + faces[bestFace].dist;
	}

	return result;
}

Physics::SweepCollision Physics::GetCapsuleAABBSweep(
	const glm::vec3& capBase, const glm::vec3& capTip, float radius,
	const glm::vec3& movement,
	const glm::vec3& boxMin, const glm::vec3& boxMax)
{
	SweepCollision result;
	const glm::vec3 capsuleMin = glm::min(capBase, capTip) - glm::vec3(radius);
	const glm::vec3 capsuleMax = glm::max(capBase, capTip) + glm::vec3(radius);
	const SweepCollision broadphase = GetAABBSweep(capsuleMin, capsuleMax, movement, boxMin, boxMax);
	if (!broadphase.hit)
	{
		return result;
	}

	const auto overlapsAt = [&](float time)
	{
		return CapsuleAABBOverlap(
			capBase + movement * time,
			capTip + movement * time,
			radius, boxMin, boxMax);
	};

	if (overlapsAt(0.0f))
	{
		const Collision collision = GetCapsuleAABBCollision(capBase, capTip, radius, boxMin, boxMax, movement);
		if (collision.hit && glm::dot(movement, collision.normal) < 0.0f)
		{
			result.hit = true;
			result.normal = collision.normal;
			result.time = 0.0f;
		}
		return result;
	}

	// The AABB broadphase can enter before the capsule itself. Search for the
	// first actual capsule overlap so the controller does not stop early.
	constexpr int sampleCount = 24;
	float previousTime = broadphase.time;
	for (int sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
	{
		const float sampleTime = broadphase.time +
			(1.0f - broadphase.time) * (static_cast<float>(sampleIndex) / static_cast<float>(sampleCount));
		if (!overlapsAt(sampleTime))
		{
			previousTime = sampleTime;
			continue;
		}

		float low = previousTime;
		float high = sampleTime;
		for (int iteration = 0; iteration < 10; ++iteration)
		{
			const float middle = (low + high) * 0.5f;
			if (overlapsAt(middle))
			{
				high = middle;
			}
			else
			{
				low = middle;
			}
		}

		const glm::vec3 contactBase = capBase + movement * high;
		const glm::vec3 contactTip = capTip + movement * high;
		const Collision collision = GetCapsuleAABBCollision(
			contactBase, contactTip, radius, boxMin, boxMax, movement);
		if (collision.hit)
		{
			result.hit = true;
			result.normal = collision.normal;
			result.time = high;
		}
		return result;
	}

	return result;
}

Physics::SweepCollision Physics::GetConvexSweep(
	const std::vector<ConvexPlane>& planes,
	const glm::vec3& center,
	const glm::vec3& movement)
{
	SweepCollision result;
	if (planes.empty())
	{
		return result;
	}

	float enterTime = 0.0f;
	float exitTime = 1.0f;
	glm::vec3 enterNormal(0.0f);
	bool startsInside = true;
	for (const ConvexPlane& plane : planes)
	{
		const float gap = plane.distance - glm::dot(plane.normal, center);
		if (gap < -0.001f)
		{
			startsInside = false;
		}

		const float normalMovement = glm::dot(plane.normal, movement);
		if (std::abs(normalMovement) <= 1e-6f)
		{
			if (gap < 0.0f)
			{
				return result;
			}
			continue;
		}

		const float crossingTime = gap / normalMovement;
		if (normalMovement < 0.0f)
		{
			if (crossingTime > enterTime)
			{
				enterTime = crossingTime;
				enterNormal = plane.normal;
			}
		}
		else
		{
			exitTime = std::min(exitTime, crossingTime);
		}
		if (enterTime > exitTime)
		{
			return result;
		}
	}

	if (startsInside)
	{
		float closestGap = std::numeric_limits<float>::max();
		for (const ConvexPlane& plane : planes)
		{
			const float gap = plane.distance - glm::dot(plane.normal, center);
			if (gap < closestGap)
			{
				closestGap = gap;
				enterNormal = plane.normal;
			}
		}
		if (glm::dot(movement, enterNormal) < 0.0f)
		{
			result.hit = true;
			result.normal = enterNormal;
			result.time = 0.0f;
		}
		return result;
	}

	if (enterTime >= 0.0f && enterTime <= 1.0f && glm::dot(enterNormal, enterNormal) > 0.0f)
	{
		result.hit = true;
		result.normal = enterNormal;
		result.time = enterTime;
	}
	return result;
}

