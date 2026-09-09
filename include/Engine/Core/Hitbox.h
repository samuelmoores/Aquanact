#pragma once

#include "Engine/Core/Component.h"
#include "Engine/Core/PhysicsCollider.h"

#include <glm/glm.hpp>
#include <string>
#include <unordered_set>
#include <utility>

class Entity;

class Hitbox final : public Component
{
public:
	~Hitbox() override;
	const char* Name() const override { return "Hitbox"; }
	int ExecutionOrder() const override { return 50; }
	void FirstFrame(Entity& owner) override;
	void Update(Entity& owner, float dt) override;

	PhysicsColliderShape Shape() const { return m_shape; }
	void SetShape(PhysicsColliderShape shape) { m_shape = shape == PhysicsColliderShape::Sphere ? shape : PhysicsColliderShape::Box; }
	float Radius() const { return m_radius; }
	void SetRadius(float radius);
	const std::string& BoneName() const { return m_boneName; }
	void SetBoneName(std::string boneName) { m_boneName = std::move(boneName); }
	bool DrawEnabled() const { return m_drawEnabled; }
	void SetDrawEnabled(bool enabled) { m_drawEnabled = enabled; }
	ColliderHandle Collider() const { return m_collider; }
	glm::vec3 DebugCenter() const;
	float Damage() const { return m_damage; }
	void SetDamage(float damage);
	float ActiveStart() const { return m_activeStart; }
	float ActiveEnd() const { return m_activeEnd; }
	void SetActiveStart(float normalizedTime);
	void SetActiveEnd(float normalizedTime);
	bool Active() const { return m_active; }
	void BeginAttack();
	void EndAttack();

	#define HITBOX_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
		BIND_FUNCTION(Active) \
		BIND_FUNCTION(Damage)
	AQUA_DECLARE_BINDABLES(HITBOX_BINDABLES)
	#undef HITBOX_BINDABLES

	#define HITBOX_EVENTS(EVENT) \
		EVENT(HitConfirmed, "Hit confirmed")
	AQUA_EVENTS_BEGIN
		HITBOX_EVENTS(AQUA_EVENT)
	AQUA_EVENTS_TEXT_END
		HITBOX_EVENTS(AQUA_EVENT_TEXT)
	AQUA_EVENTS_END
	#undef HITBOX_EVENTS

private:
	void Sync(Entity& owner);
	void UpdateAttackWindow(Entity& owner);
	void RegisterHits(Entity& owner);

	PhysicsColliderShape m_shape = PhysicsColliderShape::Sphere;
	float m_radius = 25.0f;
	std::string m_boneName;
	bool m_drawEnabled = false;
	ColliderHandle m_collider = InvalidColliderHandle;
	float m_damage = 10.0f;
	float m_activeStart = 0.2f;
	float m_activeEnd = 0.65f;
	bool m_active = false;
	std::string m_lastAnimationState;
	float m_lastStateElapsed = 0.0f;
	std::unordered_set<Entity*> m_hitTargets;
};
