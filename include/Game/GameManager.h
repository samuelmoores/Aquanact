#pragma once

#include "Engine/Core/Component.h"

// Generated gameplay class. Start here if you want to add game behavior.
//
// This class inherits from Component, so it must implement:
// - Name()
// - any lifecycle or binding hooks you need
//
// Name() tells the engine/editor what this gameplay type is called.
// GetBindableMembers() tells the engine/editor which variables or
// functions are available for UI binding later.
class GameManager final : public Component
{
public:
	GameManager() = default;

	const char* Name() const override { return "GameManager"; }
	void startUp(Entity&) override;
	void Update(Entity&, float) override {}
	void FirstFrame(Entity&) override {}
	std::vector<BindableMember> GetBindableMembers() const override;
};
