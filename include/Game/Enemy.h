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
// Example events:
//   #define Enemy_EVENTS(EVENT) \
//   	EVENT(DamageTaken, "Damage taken") \
//   	EVENT(Died, "Died")
//   AQUA_EVENTS_BEGIN
//   	Enemy_EVENTS(AQUA_EVENT)
//   AQUA_EVENTS_TEXT_END
//   	Enemy_EVENTS(AQUA_EVENT_TEXT)
//   AQUA_EVENTS_END
//
// To subscribe to an existing event at runtime, use the engine event manager:
//   const std::string channel = BindableEventChannel("DamageTaken");
//   Root::Current().Events().GetEvent(channel).Subscribe(this, [this]()
//   {
//       // React to the event here.
//   });
//
// The engine expands those lists into both the editor metadata and the
// runtime lookup code, so the UI stays in sync with the component.
class Enemy final : public Component
{
public:
	Enemy() = default;

	const char* Name() const override { return "Enemy"; }
	void Update(Entity&, float) override {}
	void FirstFrame(Entity&) override;
};
