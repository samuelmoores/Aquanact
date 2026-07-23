#include "Game/PlayerController.h"

#include <utility>

PlayerController::PlayerController(std::string name)
	: Entity(std::move(name))
{
}

const char* PlayerController::TypeName() const
{
	return "PlayerController";
}

std::vector<BindableMember> PlayerController::GetBindableMembers() const
{
	return {};
}
