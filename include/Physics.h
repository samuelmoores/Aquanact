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

	static bool CapsuleAABBOverlap(
		const glm::vec3& capBase, const glm::vec3& capTip, float radius,
		const glm::vec3& boxMin, const glm::vec3& boxMax);

	static bool SweepCapsuleAABB(
		const glm::vec3& capBase, const glm::vec3& capTip, float radius,
		const glm::vec3& movement,
		const glm::vec3& boxMin, const glm::vec3& boxMax);
};
