#include "Game/PlayerHealth.h"

#include "Engine/EventManager.h"
#include "Engine/Root.h"
#include "Engine/Entity.h"

#include <iostream>

std::string PlayerHealth::GetHealthText() const
{
	return std::to_string(m_health) + "/" + std::to_string(m_maxHealth);
}

std::vector<BindableMember> PlayerHealth::GetBindableMembers() const
{
	return {
		{ "Health", "Health", "int", BindableMember::Kind::Function },
		{ "MaxHealth", "Max Health", "int", BindableMember::Kind::Function },
	};
}

bool PlayerHealth::TryGetBindableValue(const std::string& memberName, float& value) const
{
	if (memberName == "Health")
	{
		value = static_cast<float>(m_health);
		return true;
	}
	if (memberName == "MaxHealth")
	{
		value = static_cast<float>(m_maxHealth);
		return true;
	}
	return false;
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
	Root::Current().Events().GetEvent("Damage").Subscribe(this, [this]()
	{
		SetHealth(m_health - 90);
		std::cout << "PlayerHealth received Damage and now has " << m_health << " health\n";
	});
}

void PlayerHealth::startUp(Entity&)
{
	SubscribeToDamage();
	SetHealth(m_maxHealth);
	std::cout << "PlayerHealth subbed to Damage\n";
}

void PlayerHealth::FirstFrame(Entity&)
{
}
