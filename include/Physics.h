#pragma once
#include <glm/glm.hpp>

class Physics {
public:
	Physics() = delete;

	static bool AABBOverlap(
		const glm::vec3& minA, const glm::vec3& maxA,
		const glm::vec3& minB, const glm::vec3& maxB);

	static bool SweepAABB(
		const glm::vec3& minA, const glm::vec3& maxA,
		const glm::vec3& movement,
		const glm::vec3& minB, const glm::vec3& maxB);
};
