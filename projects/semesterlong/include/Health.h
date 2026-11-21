#pragma once

#include "Engine/Core/Component.h"

class Entity;

// Reusable damage receiver for players, enemies, and other combat targets.
class Health final : public Component
{
public:
	const char* Name() const override { return "Health"; }
	void startUp(Entity&) override;
	bool ReceiveDamage(Entity& source, float amount) override;

	float CurrentHealth() const { return m_currentHealth; }
	float MaxHealth() const { return m_maxHealth; }
	void SetMaxHealth(float value);
	bool IsDead() const { return m_currentHealth <= 0.0f; }

	#define HEALTH_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
		BIND_FUNCTION(CurrentHealth) \
		BIND_FUNCTION(MaxHealth) \
		BIND_FUNCTION(IsDead)
	AQUA_DECLARE_BINDABLES(HEALTH_BINDABLES)
	#undef HEALTH_BINDABLES

	#define HEALTH_EVENTS(EVENT) \
		EVENT(DamageTaken, "Damage taken") \
		EVENT(Died, "Died")
	AQUA_EVENTS_BEGIN
		HEALTH_EVENTS(AQUA_EVENT)
	AQUA_EVENTS_TEXT_END
		HEALTH_EVENTS(AQUA_EVENT_TEXT)
	AQUA_EVENTS_END
	#undef HEALTH_EVENTS

private:
	float m_maxHealth = 100.0f;
	float m_currentHealth = 100.0f;
};
