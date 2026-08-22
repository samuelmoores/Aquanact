#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <glm/glm.hpp>

enum class LevelColliderShape
{
	Box,
	Capsule,
	// Legacy values remain readable; supported editor values are Box/Capsule.
	Sphere,
	Plane,
	ConvexMesh,
	TriangleMesh
};

// A level-owned collision object. It is intentionally independent of Entity so
// environment collision can be authored and polished without changing models.
class LevelCollider
{
public:
	explicit LevelCollider(std::string name = "LevelCollider")
		: m_name(std::move(name))
	{
	}

	const std::string& Name() const { return m_name; }
	void SetName(std::string name) { m_name = std::move(name); }

	LevelColliderShape Shape() const { return m_shape; }
	void SetShape(LevelColliderShape shape) { m_shape = shape; }

	const glm::vec3& Position() const { return m_position; }
	void SetPosition(const glm::vec3& position) { m_position = position; }
	const glm::vec3& Rotation() const { return m_rotation; }
	void SetRotation(const glm::vec3& rotation) { m_rotation = rotation; }
	const glm::vec3& Scale() const { return m_scale; }
	void SetScale(const glm::vec3& scale) { m_scale = scale; }

	float Radius() const { return m_radius; }
	void SetRadius(float radius) { m_radius = radius; }
	float Height() const { return m_height; }
	void SetHeight(float height) { m_height = height; }

	uint32_t Layer() const { return m_layer; }
	void SetLayer(uint32_t layer) { m_layer = layer; }
	uint32_t Mask() const { return m_mask; }
	void SetMask(uint32_t mask) { m_mask = mask; }

	bool IsTrigger() const { return m_isTrigger; }
	void SetTrigger(bool trigger) { m_isTrigger = trigger; }
	bool DebugVisible() const { return m_debugVisible; }
	void SetDebugVisible(bool visible) { m_debugVisible = visible; }

private:
	std::string m_name;
	LevelColliderShape m_shape = LevelColliderShape::Box;
	glm::vec3 m_position{ 0.0f };
	glm::vec3 m_rotation{ 0.0f };
	glm::vec3 m_scale{ 1.0f };
	float m_radius = 50.0f;
	float m_height = 100.0f;
	uint32_t m_layer = 1u;
	uint32_t m_mask = 0xFFFFFFFFu;
	bool m_isTrigger = false;
	bool m_debugVisible = true;
};
