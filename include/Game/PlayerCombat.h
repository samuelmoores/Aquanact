#pragma once

#include "Engine/Core/Component.h"

class Entity;
class EntityStateMachine;
class Input;
class InputManager;
class Root;

// Generated gameplay component scaffold.
//
// Contract:
// - Name() identifies the component type for the factory and editor
// - startUp/Update/FirstFrame provide the runtime lifecycle hooks
// - the bindable macros expose values and one-shot events to the UI
//
// Keep the binding lists as the single source of truth for exposed data.
// The macros generate both editor metadata and lookup code.
//
// Example values:
//   #define PlayerCombat_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
//   	BIND_VALUE(m_health) \
//   	BIND_FUNCTION(Health)
//   AQUA_DECLARE_BINDABLES(PlayerCombat_BINDABLES)
//   #undef PlayerCombat_BINDABLES
//
// Example events:
//   #define PlayerCombat_EVENTS(EVENT) \
//   	EVENT(HealthChanged, "Health changed") \
//   	EVENT(Died, "Died")
//   AQUA_EVENTS_BEGIN
//   	PlayerCombat_EVENTS(AQUA_EVENT)
//   AQUA_EVENTS_TEXT_END
//   	PlayerCombat_EVENTS(AQUA_EVENT_TEXT)
//   AQUA_EVENTS_END
class PlayerCombat final : public Component
{
public:
	PlayerCombat() = default;

	const char* Name() const override { return "PlayerCombat"; }
	int ExecutionOrder() const override { return -50; }
	void startUp(Entity&) override;
	void FirstFrame(Entity&) override;
	void Update(Entity&, float) override;

	//members
	bool m_attackRequested = false;

	// External input sources.
	const Input* m_inputDevice = nullptr;
	const InputManager* m_inputActions = nullptr;
	EntityStateMachine* m_entityState = nullptr;

	// Put the component's exposed value list here. This is the only place
	// that should enumerate values the editor needs to see.
	#define PlayerCombat_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
			BIND_VALUE(m_attackRequested)
		AQUA_DECLARE_BINDABLES(PlayerCombat_BINDABLES)
	#undef PlayerCombat_BINDABLES
};
