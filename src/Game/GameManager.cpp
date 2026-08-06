#include "Game/GameManager.h"

#include <utility>

GameManager::GameManager(std::string name)
	: Entity(std::move(name))
{
}

const char* GameManager::TypeName() const
{
	return "GameManager";
}

std::vector<BindableMember> GameManager::GetBindableMembers() const
{
	return {};
}
