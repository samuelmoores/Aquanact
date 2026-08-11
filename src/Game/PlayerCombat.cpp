#include "Game/PlayerCombat.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/Root.h"

void PlayerCombat::startUp(Entity&)
{
	// Attack requests are runtime-only and must not survive a session restart.
	m_attackRequested = false;
	m_inputActions = &Root::Current().InputActions();
}

void PlayerCombat::FirstFrame(Entity& owner)
{
	m_entityState = owner.GetComponent<EntityStateMachine>();
}

void PlayerCombat::Update(Entity&, float)
{
	if (m_inputActions->WasPressed("Attack"))
		m_attackRequested = true;
	else
		m_attackRequested = false;

	if (m_attackRequested)
		std::cout << "attack requested from player combat\n";
}

// Keep component metadata in the header with the binding/event list macros.
// Add runtime logic here only if the component needs it.
