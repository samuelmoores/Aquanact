#include "Engine/Core/ComponentRegistry.h"

#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Game/AIController.h"
#include "Game/Health.h"
#include "Game/PlayerCombat.h"
#include "Game/PlayerAttack.h"
#include "Game/PlayerController.h"
#include "Game/PlayerHealth.h"
#include "Engine/Core/TriggerSphere.h"
#include "Engine/Core/Hitbox.h"

#include <memory>

void RegisterGameComponents()
{
	ComponentFactory::Instance().Register("EntityStateMachine", [](Entity& owner) -> std::unique_ptr<Component>
	{
		return owner.GetMesh() && owner.GetMesh()->Skinned()
			? std::make_unique<EntityStateMachine>(owner.GetMesh())
			: nullptr;
	});
	ComponentFactory::Instance().Register("Controller", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Controller>();
	});
	ComponentFactory::Instance().Register("AIController", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<AIController>();
	});
	ComponentFactory::Instance().Register("Health", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Health>();
	});
	ComponentFactory::Instance().Register("PlayerCombat", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<PlayerCombat>();
	});
	ComponentFactory::Instance().Register("PlayerAttack", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<PlayerAttack>();
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
	ComponentFactory::Instance().Register("Hitbox", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Hitbox>();
	});
}
