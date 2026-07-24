#include "Game/Enemy.h"

#include "Engine/EventManager.h"
#include "Engine/Globals.h"

#include <iostream>

void Enemy::FirstFrame(Entity&)
{
	std::cout << "Enemy dispatching Damage event\n";
	gEventManager.Dispatch("Damage");
}
