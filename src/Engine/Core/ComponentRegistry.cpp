#include "Engine/Core/ComponentRegistry.h"

#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/ParticleSystem.h"
#include "Game/AIController.h"
#include "Game/AttackSphere.h"
#include "Game/Health.h"
#include "Game/PlayerAttack.h"
#include "Game/PlayerCombat.h"
#include "Game/PlayerController.h"
#include "Game/PlayerHealth.h"

#include <memory>

void RegisterGameComponents()
{
	ComponentFactory::Instance().Register("EntityStateMachine", [](Entity& owner) -> std::unique_ptr<Component>
	{
		return std::make_unique<EntityStateMachine>(owner.GetMesh());
	});
	ComponentFactory::Instance().Register("ParticleSystem", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<ParticleSystem>();
	});
	ComponentFactory::Instance().Register("Controller", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Controller>();
	});
	ComponentFactory::Instance().Register("AIController", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<AIController>();
	});
	ComponentFactory::Instance().Register("AttackSphere", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<AttackSphere>();
	});
	ComponentFactory::Instance().Register("Health", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Health>();
	});
	ComponentFactory::Instance().Register("PlayerAttack", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<PlayerAttack>();
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
}
