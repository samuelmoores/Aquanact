#include "Game/GameManagohala.h"

#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Entity.h"

#include <memory>

namespace
{
	const bool registeredGameManagohala = []()
	{
		ComponentFactory::Instance().Register("GameManagohala", [](Entity&) -> std::unique_ptr<Component>
		{
			return std::unique_ptr<Component>(new GameManagohala());
		});
		return true;
	}();
}

void GameManagohala::startUp(Entity&)
{
}

std::vector<BindableMember> GameManagohala::GetBindableMembers() const
{
	return {};
}
