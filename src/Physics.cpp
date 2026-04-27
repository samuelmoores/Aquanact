#include "Physics.h"
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
