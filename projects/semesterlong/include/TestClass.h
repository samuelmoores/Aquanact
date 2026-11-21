#pragma once

#include "Engine/Core/Component.h"

class Entity;
class EntityStateMachine;
class Input;
class InputManager;
class Root;

// Generated gameplay component scaffold.
// Keep the binding lists as the single source of truth for exposed data.
// Add bindable values and events here when the component needs editor metadata.
class TestClass final : public Component
{
public:
	TestClass() = default;

	const char* Name() const override { return "TestClass"; }
	void startUp(Entity&) override;
	void Update(Entity&, float) override {}
	void FirstFrame(Entity&) override {}

	// Put the component's exposed value list here. This is the only place
	// that should enumerate values the editor needs to see.
};
