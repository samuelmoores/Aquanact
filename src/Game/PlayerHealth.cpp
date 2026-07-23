#include "Game/PlayerHealth.h"

#include "Engine/EventManager.h"

#include <iostream>
#include <utility>

PlayerHealth::PlayerHealth(std::string name)
	: GameObject(std::move(name))
{
}

std::vector<BindableMember> PlayerHealth::GetBindableMembers() const
{
	return {
		{ "health", "Health", "int", BindableMember::Kind::Variable },
		{ "maxHealth", "Max Health", "int", BindableMember::Kind::Variable },
		{ "GetHealthText", "Health Text", "string", BindableMember::Kind::Function },
	};
}

std::string PlayerHealth::GetHealthText() const
{
	return std::to_string(m_health) + "/" + std::to_string(m_maxHealth);
}

void PlayerHealth::SubscribeToDamage(EventManager& events)
{
	events.GetEvent("Damage").Subscribe(this, [this]()
	{
		m_health -= 25;
		if (m_health < 0)
		{
			m_health = 0;
		}
		std::cout << "PlayerHealth received Damage and now has " << m_health << " health\n";
	});
}
