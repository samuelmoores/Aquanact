#include "Engine/Core/ComponentRegistry.h"

#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Game/Enemy.h"
#include "Game/PlayerCombat.h"
#include "Game/PlayerController.h"
#include "Game/PlayerHealth.h"
#include "Engine/Core/TriggerSphere.h"

#include <memory>

void RegisterGameComponents()
{
	ComponentFactory::Instance().Register("Controller", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Controller>();
	});
	ComponentFactory::Instance().Register("Enemy", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Enemy>();
	});
	ComponentFactory::Instance().Register("PlayerCombat", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<PlayerCombat>();
	});
	ComponentFactory::Instance().Register("PlayerController", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<PlayerController>();
	});
	ComponentFactory::Instance().Register("PlayerHealth", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<PlayerHealth>();
	});
	ComponentFactory::Instance().Register("TriggerSphere", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<TriggerSphere>();
	});
}
