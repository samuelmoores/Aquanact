#include "Game/Enemy.h"

#include "Engine/EventManager.h"
#include "Engine/Root.h"

#include <iostream>

void Enemy::FirstFrame(Entity&)
{
	std::cout << "Enemy dispatching Damage event\n";
	Root::Current().Events().Dispatch("Damage");
}
