#include "Engine/Physics.h"
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

