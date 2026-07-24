#include "Game/PlayerHealth.h"

#include "Engine/EventManager.h"
#include "Engine/Globals.h"
#include "Engine/Entity.h"

#include <iostream>

std::string PlayerHealth::GetHealthText() const
{
	return std::to_string(m_health) + "/" + std::to_string(m_maxHealth);
}

void PlayerHealth::SubscribeToDamage()
{
	gEventManager.GetEvent("Damage").Subscribe(this, [this]()
	{
		m_health -= 25;
		if (m_health < 0)
		{
			m_health = 0;
		}
		std::cout << "PlayerHealth received Damage and now has " << m_health << " health\n";
	});
}

void PlayerHealth::FirstFrame(Entity&)
{
	SubscribeToDamage();
}
