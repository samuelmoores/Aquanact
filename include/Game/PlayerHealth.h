#pragma once

#include "Engine/Component.h"

#include <string>

class PlayerHealth final : public Component
{
public:
	PlayerHealth() = default;

	const char* Name() const override { return "PlayerHealth"; }
	void startUp(Entity&) override;
	void Update(Entity&, float) override {}
	void FirstFrame(Entity&) override;
	std::vector<BindableEvent> GetBindableEvents() const override;
	std::string GetBindableEventText(const std::string& eventName) const override;

	int Health() const { return m_health; }
	void SetHealth(int health);

	int MaxHealth() const { return m_maxHealth; }
	void SetMaxHealth(int maxHealth);

	std::string GetHealthText() const;
	void SubscribeToDamage();

private:
	int m_health = 100;
	int m_maxHealth = 100;
};
