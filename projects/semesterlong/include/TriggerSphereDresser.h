#pragma once

#include "Engine/Core/TriggerSphere.h"

class Entity;

class TriggerSphereDresser final : public TriggerSphere
{
public:
	const char* Name() const override { return "TriggerSphereDresser"; }
	void OnEntityEnter(Entity& triggerOwner, Entity& enteringEntity) override;
	void OnEntityExit(Entity& triggerOwner, Entity& leavingEntity) override;
};
