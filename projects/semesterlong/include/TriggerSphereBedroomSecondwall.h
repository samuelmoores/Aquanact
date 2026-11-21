#pragma once

#include "Engine/Core/TriggerSphere.h"

class Entity;

class TriggerSphereBedroomSecondwall final : public TriggerSphere
{
public:
	const char* Name() const override { return "TriggerSphereBedroomSecondwall"; }
	void OnEntityEnter(Entity& triggerOwner, Entity& enteringEntity) override;
	void OnEntityExit(Entity& triggerOwner, Entity& leavingEntity) override;
};
