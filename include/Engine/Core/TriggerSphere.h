#pragma once

#include "Engine/Core/Component.h"

#include <unordered_set>

class Entity;

class TriggerSphere final : public Component {
public:
	const char* Name() const override { return "TriggerSphere"; }
	void startUp(Entity& owner) override;
	void Update(Entity& owner, float deltaTime) override;

	void SetRadius(float radius);
	float Radius() const { return m_radius; }

	void SetEnabled(bool enabled) { m_enabled = enabled; }
	bool Enabled() const { return m_enabled; }

	#define TRIGGER_SPHERE_BINDABLES(VALUE, FUNCTION) \
		VALUE(m_radius) \
		VALUE(m_enabled)
	AQUA_DECLARE_BINDABLES(TRIGGER_SPHERE_BINDABLES)
	#undef TRIGGER_SPHERE_BINDABLES

private:
	float m_radius = 90.0f;
	bool m_enabled = true;
	std::unordered_set<Entity*> m_overlapping;
};
