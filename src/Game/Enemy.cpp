#include "Game/Enemy.h"

#include "Engine/Core/EventManager.h"
#include "Engine/Core/Root.h"

#include <iostream>

void Enemy::FirstFrame(Entity&)
{
	std::cout << "Enemy dispatching Damage event\n";
	Root::Current().Events().Dispatch("Damage");
}
