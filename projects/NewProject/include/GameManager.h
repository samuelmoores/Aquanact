#pragma once

#include "Engine/Core/Component.h"

class Entity;

// Project-owned gameplay component. Add project-specific behavior here.
class GameManager final : public Component
{
public:
	const char* Name() const override { return "GameManager"; }
};
