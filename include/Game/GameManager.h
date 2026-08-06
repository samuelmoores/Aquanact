#pragma once

#include "Engine/Entity.h"

// Generated gameplay class. Start here if you want to add game behavior.
//
// This class inherits from Entity, so it must implement:
// - TypeName()
// - GetBindableMembers()
//
// TypeName() tells the engine/editor what this gameplay type is called.
// GetBindableMembers() tells the engine/editor which variables or
// functions are available for UI binding later.
class GameManager final : public Entity
{
public:
	explicit GameManager(std::string name = "GameManager");

	const char* TypeName() const override;
	std::vector<BindableMember> GetBindableMembers() const override;
};
