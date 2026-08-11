#include "Game/ComponentRegistry.h"

#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <memory>

void RegisterGameComponents()
{
	ComponentFactory::Instance().Register("Controller", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Controller>();
	});
	ComponentFactory::Instance().Register("EntityStateMachine", [](Entity& owner) -> std::unique_ptr<Component>
	{
		return std::make_unique<EntityStateMachine>(owner.GetMesh());
	});
	ComponentFactory::Instance().Register("Enemy", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Enemy>();
	});
	ComponentFactory::Instance().Register("PlayerHealth", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<PlayerHealth>();
	});
}
