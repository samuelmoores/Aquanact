#pragma once

#include "Engine/Core/Component.h"

#include <string>

class Entity;
class EntityStateMachine;
class InputManager;

// Converts Attack input into a buffered, bindable state-machine trigger.
// Configure both Idle -> Attack and Attack -> Attack to test
// PlayerAttack.m_attackTrigger == true.
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

private:
	bool m_attackTrigger = false;
	const InputManager* m_inputActions = nullptr;
	EntityStateMachine* m_entityState = nullptr;
	std::string m_lastState;
	float m_lastStateElapsed = 0.0f;
};
