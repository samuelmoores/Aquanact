#include "Game/PlayerHealth.h"

#include "Engine/EventManager.h"
#include "Engine/Globals.h"
#include "Engine/Entity.h"

#include <iostream>

std::string PlayerHealth::GetHealthText() const
{
	return std::to_string(m_health) + "/" + std::to_string(m_maxHealth);
}

std::vector<BindableEvent> PlayerHealth::GetBindableEvents() const
{
	return {
		{ "HealthChanged", "Health Changed" }
	};
}

std::string PlayerHealth::GetBindableEventText(const std::string& eventName) const
{
	if (eventName == "HealthChanged")
	{
		return GetHealthText();
	}
	return {};
}

void PlayerHealth::SetHealth(int health)
{
	m_health = health;
	if (m_health < 0)
	{
		m_health = 0;
	}
	DispatchBindableEvent("HealthChanged");
}

void PlayerHealth::SetMaxHealth(int maxHealth)
{
	m_maxHealth = maxHealth;
	DispatchBindableEvent("HealthChanged");
}

void PlayerHealth::SubscribeToDamage()
{
	gEventManager.GetEvent("Damage").Subscribe(this, [this]()
	{
		SetHealth(m_health - 25);
		std::cout << "PlayerHealth received Damage and now has " << m_health << " health\n";
	});
}

void PlayerHealth::startUp(Entity&)
{
	SubscribeToDamage();
	std::cout << "PlayerHealth subbed to Damage\n";
}

void PlayerHealth::FirstFrame(Entity&)
{
	
}
