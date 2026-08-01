#pragma once

#include "Engine/Core/Component.h"

class Enemy final : public Component
{
public:
	Enemy() = default;

	const char* Name() const override { return "Enemy"; }
	void Update(Entity&, float) override {}
	void FirstFrame(Entity&) override;
};
