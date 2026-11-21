#pragma once

#include "Engine/Core/Component.h"

#include <glm/glm.hpp>
#include <string>

class Entity;
class InputManager;

// Spawns and positions the configured attack instance from the Attack input.
class PlayerAttack final : public Component
{
public:
	PlayerAttack() = default;

	const char* Name() const override { return "PlayerAttack"; }
	int ExecutionOrder() const override { return -50; }
	void startUp(Entity&) override;
	void Update(Entity&, float) override;

	#define PLAYER_ATTACK_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
		BIND_VALUE(m_attackTrigger)
	AQUA_DECLARE_BINDABLES(PLAYER_ATTACK_BINDABLES)
	#undef PLAYER_ATTACK_BINDABLES

	const std::string& AttackInstanceName() const { return m_attackInstanceName; }
	void SetAttackInstanceName(std::string name) { m_attackInstanceName = std::move(name); }
	float AttackInstanceSpeed() const { return m_attackInstanceSpeed; }
	void SetAttackInstanceSpeed(float speed) { m_attackInstanceSpeed = speed; }
	int AttackLaunchFrame() const { return m_attackLaunchFrame; }
	void SetAttackLaunchFrame(int frame) { m_attackLaunchFrame = frame < 0 ? 0 : frame; }
	const glm::vec3& AttackInstanceOffset() const { return m_attackInstanceOffset; }
	void SetAttackInstanceOffset(glm::vec3 offset) { m_attackInstanceOffset = offset; }
	const std::string& AttackInstanceBoneName() const { return m_attackInstanceBoneName; }
	void SetAttackInstanceBoneName(std::string boneName) { m_attackInstanceBoneName = std::move(boneName); }
	bool DrawAttackInstance() const { return m_drawAttackInstance; }
	void SetDrawAttackInstance(bool draw) { m_drawAttackInstance = draw; }
	const std::string& AttackTargetInstanceName() const { return m_attackTargetInstanceName; }
	void SetAttackTargetInstanceName(std::string name) { m_attackTargetInstanceName = std::move(name); }
	const std::string& AttackTargetBoneName() const { return m_attackTargetBoneName; }
	void SetAttackTargetBoneName(std::string name) { m_attackTargetBoneName = std::move(name); }

private:
	glm::vec3 AttackInstanceSpawnPosition(Entity& owner) const;

	float m_attackTrigger = 0.0f;
	const InputManager* m_inputActions = nullptr;
	std::string m_attackInstanceName = "SpellSphere";
	Entity* m_attackInstance = nullptr;
	glm::vec3 m_attackInstanceOffset{0.0f, 0.0f, -35.0f};
	std::string m_attackInstanceBoneName;
	float m_attackInstanceSpeed = 120.0f;
	int m_attackLaunchFrame = 0;
	bool m_drawAttackInstance = false;
	std::string m_attackTargetInstanceName;
	std::string m_attackTargetBoneName;
	float m_attackSequenceElapsed = 0.0f;
	bool m_attackSequenceActive = false;
	bool m_attackUsed = false;
};
