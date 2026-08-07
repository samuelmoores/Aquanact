#include "Game/GameManager.h"

#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Entity.h"
#include <memory>

namespace
{
	const bool registeredGameManager = []()
	{
		ComponentFactory::Instance().Register("GameManager", [](Entity&) -> std::unique_ptr<Component>
		{
			return std::unique_ptr<Component>(new GameManager());
		});
		return true;
	}();
}

void GameManager::startUp(Entity&)
{
}

std::vector<BindableMember> GameManager::GetBindableMembers() const
{
	return {};
}
