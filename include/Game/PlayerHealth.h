#pragma once

#include "Engine/Core/Component.h"

// Example gameplay component.
//
// Contract:
// - Name() identifies the component type
// - startUp/Update/FirstFrame are the runtime lifecycle hooks
// - the binding list exposes editor-visible state
//
// Keep the binding list as the single source of truth for exposed values.
//
// Example values:
//   #define PlayerHealth_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
//   	BIND_VALUE(m_health) \
//   	BIND_FUNCTION(Health)
//   AQUA_DECLARE_BINDABLES(PlayerHealth_BINDABLES)
//   #undef PlayerHealth_BINDABLES
//
// Example events:
//   #define PlayerHealth_EVENTS(EVENT) \
//   	EVENT(HealthChanged, "Health changed") \
//   	EVENT(Died, "Died")
//   AQUA_EVENTS_BEGIN
//   	PlayerHealth_EVENTS(AQUA_EVENT)
//   AQUA_EVENTS_TEXT_END
//   	PlayerHealth_EVENTS(AQUA_EVENT_TEXT)
//   AQUA_EVENTS_END
//
// To subscribe to an existing event at runtime, use the engine event manager:
//   const std::string channel = BindableEventChannel("Died");
//   Root::Current().Events().GetEvent(channel).Subscribe(this, [this]()
//   {
//       // React to the event here.
//   });
//
// The engine expands those lists into both the editor metadata and the
// runtime lookup code, so the UI stays in sync with the component.
class PlayerHealth final : public Component
{
public:
	PlayerHealth() = default;

	const char* Name() const override { return "PlayerHealth"; }
	void startUp(Entity&) override;
	void Update(Entity&, float) override {}
	void FirstFrame(Entity&) override {}

	// Expose the values the UI should read through the binding list below.
	#define PLAYER_HEALTH_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
		BIND_VALUE(m_health)
	AQUA_DECLARE_BINDABLES(PLAYER_HEALTH_BINDABLES)
	#undef PLAYER_HEALTH_BINDABLES

private:
	float m_health = 100.0f;
};
