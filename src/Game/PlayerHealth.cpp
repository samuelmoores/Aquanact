#include "Game/PlayerHealth.h"
#include <utility>

PlayerHealth::PlayerHealth(std::string name)
	: GameObject(std::move(name))
{
}

std::vector<BindableMember> PlayerHealth::GetBindableMembers() const
{
	return {
		{ "health", "Health", "int", BindableMember::Kind::Property },
		{ "maxHealth", "Max Health", "int", BindableMember::Kind::Property },
		{ "GetHealthText", "Health Text", "string", BindableMember::Kind::Function },
	};
}

std::string PlayerHealth::GetHealthText() const
{
	return std::to_string(m_health) + "/" + std::to_string(m_maxHealth);
}
