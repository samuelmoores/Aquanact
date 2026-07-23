#pragma once

#include "Engine/Entity.h"

class EventManager;

// Example child class that shows the minimum required shape.
//
// To compile successfully, any new Entity child must:
// - inherit from Entity
// - implement TypeName()
// - implement GetBindableMembers()
// - construct the Entity base with a name
//
// The actual gameplay data (`health`, `maxHealth`) is private here, but the
// class exposes getters/setters and reports those members in
// GetBindableMembers() so other systems can inspect them later.
class PlayerHealth final : public Entity
{
public:
	explicit PlayerHealth(std::string name = "PlayerHealth");

	const char* TypeName() const override { return "PlayerHealth"; }
	std::vector<BindableMember> GetBindableMembers() const override;

	int Health() const { return m_health; }
	void SetHealth(int health) { m_health = health; }

	int MaxHealth() const { return m_maxHealth; }
	void SetMaxHealth(int maxHealth) { m_maxHealth = maxHealth; }

	std::string GetHealthText() const;
	void SubscribeToDamage(EventManager& events);

private:
	int m_health = 100;
	int m_maxHealth = 100;
};
