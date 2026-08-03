#pragma once
#include <glm/glm.hpp>
#include <vector>

class Physics {
public:
	Physics() = delete;

	struct Collision {
		bool hit;
		glm::vec3 normal;
		float penetration;
	};

	struct SweepCollision {
		bool hit = false;
		glm::vec3 normal{ 0.0f };
		float time = 1.0f;
	};

	struct ConvexPlane {
		glm::vec3 normal{ 0.0f };
		float distance = 0.0f;
	};

	static bool AABBOverlap(
		const glm::vec3& minA, const glm::vec3& maxA,
		const glm::vec3& minB, const glm::vec3& maxB);

	static bool SweepAABB(
		const glm::vec3& minA, const glm::vec3& maxA,
		const glm::vec3& movement,
		const glm::vec3& minB, const glm::vec3& maxB);

	static SweepCollision GetAABBSweep(
		const glm::vec3& minA, const glm::vec3& maxA,
		const glm::vec3& movement,
		const glm::vec3& minB, const glm::vec3& maxB);

	static bool SphereAABBOverlap(
		const glm::vec3& center, float radius,
		const glm::vec3& boxMin, const glm::vec3& boxMax);

	static bool SweepSphereAABB(
		const glm::vec3& center, float radius,
		const glm::vec3& movement,
		const glm::vec3& boxMin, const glm::vec3& boxMax);

	static SweepCollision GetSphereAABBSweep(
		const glm::vec3& center, float radius,
		const glm::vec3& movement,
		const glm::vec3& boxMin, const glm::vec3& boxMax);

	static Collision GetSphereAABBCollision(
		const glm::vec3& center, float radius,
		const glm::vec3& boxMin, const glm::vec3& boxMax,
		const glm::vec3& movement);

	static bool CapsuleAABBOverlap(
		const glm::vec3& capBase, const glm::vec3& capTip, float radius,
		const glm::vec3& boxMin, const glm::vec3& boxMax);

	static bool SweepCapsuleAABB(
		const glm::vec3& capBase, const glm::vec3& capTip, float radius,
		const glm::vec3& movement,
		const glm::vec3& boxMin, const glm::vec3& boxMax);

	static Collision GetCapsuleAABBCollision(
		const glm::vec3& capBase, const glm::vec3& capTip, float radius,
		const glm::vec3& boxMin, const glm::vec3& boxMax,
		const glm::vec3& movement);

	static SweepCollision GetCapsuleAABBSweep(
		const glm::vec3& capBase, const glm::vec3& capTip, float radius,
		const glm::vec3& movement,
		const glm::vec3& boxMin, const glm::vec3& boxMax);

	static SweepCollision GetConvexSweep(
		const std::vector<ConvexPlane>& planes,
		const glm::vec3& center,
		const glm::vec3& movement);
};

