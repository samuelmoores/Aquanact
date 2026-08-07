#pragma once

#include "Engine/Core/Component.h"

// Generated gameplay component. Start here if you want to add game behavior.
//
// This class inherits from Component, so it must implement:
// - Name()
// - any lifecycle or binding hooks you need
class GameManagohala final : public Component
{
public:
	GameManagohala() = default;

	const char* Name() const override { return "GameManagohala"; }
	void startUp(Entity&) override;
	void Update(Entity&, float) override {}
	void FirstFrame(Entity&) override {}
	std::vector<BindableMember> GetBindableMembers() const override;
};
