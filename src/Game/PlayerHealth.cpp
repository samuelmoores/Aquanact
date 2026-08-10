#include "Game/PlayerHealth.h"

void PlayerHealth::startUp(Entity&)
{
	m_health = 100.0f;
}

// Bindable metadata stays in the header through the PlayerHealth_BINDABLES
// macro list and AQUA_DECLARE_BINDABLES(...). This source file stays empty
// unless the component needs startup logic or custom runtime behavior.
