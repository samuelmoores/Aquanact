#include "Physics.h"

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
