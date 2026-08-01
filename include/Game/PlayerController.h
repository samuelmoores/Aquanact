#pragma once

#include "Engine/Core/Entity.h"

// Generated gameplay class. Start here if you want to add game behavior.
//
// This class inherits from Entity, so it must implement:
// - TypeName()
// - GetBindableMembers()
//
// TypeName() tells the engine/editor what this gameplay type is called.
// GetBindableMembers() tells the engine/editor which variables or
// functions are available for UI binding later.
class PlayerController final : public Entity
{
public:
	explicit PlayerController(std::string name = "PlayerController");

	const char* TypeName() const override;
	std::vector<BindableMember> GetBindableMembers() const override;
};
