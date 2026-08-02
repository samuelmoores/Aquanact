#include "Engine/Core/Physics.h"
#include <algorithm>
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
	return AABBOverlap(minA + movement, maxA + movement, minB, maxB);
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

