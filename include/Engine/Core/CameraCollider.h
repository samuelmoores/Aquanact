#pragma once

#include "Engine/Core/Physics.h"

#include <glm/glm.hpp>

class CameraCollider final
{
public:
	 explicit CameraCollider(float radius = 25.0f) : m_radius(radius) {}

	void SetPosition(const glm::vec3& position) { m_position = position; }
	void SetRadius(float radius) { m_radius = glm::max(radius, 0.001f); }
	const glm::vec3& Position() const { return m_position; }
	float Radius() const { return m_radius; }

	bool OverlapsAABB(const glm::vec3& boxMin, const glm::vec3& boxMax) const
	{
		return Physics::SphereAABBOverlap(m_position, m_radius, boxMin, boxMax);
	}

	bool SweepsIntoAABB(const glm::vec3& movement, const glm::vec3& boxMin, const glm::vec3& boxMax) const
	{
		return Physics::SweepSphereAABB(m_position, m_radius, movement, boxMin, boxMax);
	}

	Physics::SweepCollision SweepAgainstAABB(
		const glm::vec3& movement, const glm::vec3& boxMin, const glm::vec3& boxMax) const
	{
		return Physics::GetSphereAABBSweep(m_position, m_radius, movement, boxMin, boxMax);
	}

	Physics::Collision CollisionAgainstAABB(
		const glm::vec3& boxMin, const glm::vec3& boxMax,
		const glm::vec3& movement = glm::vec3(0.0f)) const
	{
		return Physics::GetSphereAABBCollision(m_position, m_radius, boxMin, boxMax, movement);
	}

private:
	glm::vec3 m_position{0.0f};
	float m_radius = 25.0f;
};
