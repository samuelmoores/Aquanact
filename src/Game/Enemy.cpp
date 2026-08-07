#include "Game/Enemy.h"

#include "Engine/Core/EventManager.h"
#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Root.h"

#include <iostream>

namespace
{
	const bool registeredEnemy = []()
	{
		ComponentFactory::Instance().Register("Enemy", [](Entity&) -> std::unique_ptr<Component>
		{
			return std::unique_ptr<Component>(new Enemy());
		});
		return true;
	}();
}

void Enemy::FirstFrame(Entity&)
{
	std::cout << "Enemy dispatching Damage event\n";
	Root::Current().Events().Dispatch("Damage");
}
