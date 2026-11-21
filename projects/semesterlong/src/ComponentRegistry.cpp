#include "Engine/Core/ComponentRegistry.h"

#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/ParticleSystem.h"
#include "Engine/Core/TriggerSphere.h"
#include "AIController.h"
#include "AttackSphere.h"
#include "CharacterEditor.h"
#include "GameManager.h"
#include "Health.h"
#include "PlayerAttack.h"
#include "PlayerCombat.h"
#include "PlayerController.h"
#include "PlayerHealth.h"
#include "TestClass.h"
#include "TriggerSphereBedroomSecondwall.h"
#include "TriggerSphereDresser.h"

#include <memory>

void RegisterGameComponents()
{
	ComponentFactory::Instance().Register("EntityStateMachine", [](Entity& owner) -> std::unique_ptr<Component>
	{
		return std::make_unique<EntityStateMachine>(owner.GetMesh());
	});
	ComponentFactory::Instance().Register("Controller", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<Controller>();
	});
	ComponentFactory::Instance().Register("ParticleSystem", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<ParticleSystem>();
	});
	ComponentFactory::Instance().Register("TriggerSphere", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<TriggerSphere>();
	});
	ComponentFactory::Instance().Register("AIController", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<AIController>();
	});
	ComponentFactory::Instance().Register("AttackSphere", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<AttackSphere>();
	});
	ComponentFactory::Instance().Register("GameManager", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<GameManager>();
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
	ComponentFactory::Instance().Register("CharacterEditor", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<CharacterEditor>();
	});
	ComponentFactory::Instance().Register("PlayerHealth", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<PlayerHealth>();
	});
	ComponentFactory::Instance().Register("TestClass", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<TestClass>();
	});
	ComponentFactory::Instance().Register("TriggerSphereBedroomSecondwall", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<TriggerSphereBedroomSecondwall>();
	});
	ComponentFactory::Instance().Register("TriggerSphereDresser", [](Entity&) -> std::unique_ptr<Component>
	{
		return std::make_unique<TriggerSphereDresser>();
	});
}
