#include "Engine/Core/PhysicsWorld.h"

PhysicsWorld& PhysicsWorld::Instance()
{
	static PhysicsWorld world;
	return world;
}
