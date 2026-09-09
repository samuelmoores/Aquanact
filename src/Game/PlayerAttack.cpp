#include "Game/PlayerAttack.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/Root.h"

void PlayerAttack::startUp(Entity& owner)
{
	m_attackTrigger = false;
	m_inputActions = &Root::Current().InputActions();
	m_entityState = owner.GetComponent<EntityStateMachine>();
	m_lastState.clear();
	m_lastStateElapsed = 0.0f;
}

void PlayerAttack::Update(Entity& owner, float)
{
	if (!m_inputActions)
	{
		m_inputActions = &Root::Current().InputActions();
	}
	if (!m_entityState)
	{
		m_entityState = owner.GetComponent<EntityStateMachine>();
	}

	const std::string currentState = m_entityState ? m_entityState->CurrentState() : std::string{};
	const float currentStateElapsed = m_entityState
		? m_entityState->CurrentStateElapsedSeconds() : 0.0f;
	const bool stateRestarted = currentState != m_lastState
		|| currentStateElapsed + 0.0001f < m_lastStateElapsed;
	if (stateRestarted)
	{
		// A state restart means the previous request was consumed by a transition.
		// Clearing here still allows a new press on the same frame to be queued.
		m_attackTrigger = false;
	}

	// Keep the request latched until the state machine restarts a state. This
	// lets an Attack -> Attack transition consume a press made during the
	// current attack instead of losing the one-frame input edge.
	if (m_inputActions->WasPressed("Attack"))
	{
		m_attackTrigger = true;
	}

	m_lastState = currentState;
	m_lastStateElapsed = currentStateElapsed;
}
