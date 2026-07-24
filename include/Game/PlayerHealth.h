#pragma once

#include "Engine/Component.h"

#include <string>

class PlayerHealth final : public Component
{
public:
	PlayerHealth() = default;

	const char* Name() const override { return "PlayerHealth"; }
	void Update(Entity&, float) override {}
	void FirstFrame(Entity&) override;

	int Health() const { return m_health; }
	void SetHealth(int health) { m_health = health; }

	int MaxHealth() const { return m_maxHealth; }
	void SetMaxHealth(int maxHealth) { m_maxHealth = maxHealth; }

	std::string GetHealthText() const;
	void SubscribeToDamage();

private:
	int m_health = 100;
	int m_maxHealth = 100;
};
