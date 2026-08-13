#pragma once

#include "Engine/Core/Component.h"

#include <functional>
#include <utility>
#include <unordered_set>

class Entity;

class TriggerSphere final : public Component {
public:
	using EnterCallback = std::function<void(Entity& triggerOwner, Entity& enteredEntity)>;

	const char* Name() const override { return "TriggerSphere"; }
	void startUp(Entity& owner) override;
	void Update(Entity& owner, float deltaTime) override;

	void SetRadius(float radius);
	float Radius() const { return m_radius; }

	void SetEnabled(bool enabled) { m_enabled = enabled; }
	bool Enabled() const { return m_enabled; }

	void SetOnEnter(EnterCallback callback) { m_onEnter = std::move(callback); }

	AQUA_EVENTS_BEGIN
		AQUA_EVENT(Entered, "An entity entered the trigger sphere")
	AQUA_EVENTS_TEXT_END
		AQUA_EVENT_TEXT(Entered, "An entity entered the trigger sphere")
		AQUA_EVENTS_END

	#define TRIGGER_SPHERE_BINDABLES(VALUE, FUNCTION) \
		VALUE(m_radius) \
		VALUE(m_enabled)
	AQUA_DECLARE_BINDABLES(TRIGGER_SPHERE_BINDABLES)
	#undef TRIGGER_SPHERE_BINDABLES

private:
	float m_radius = 1.0f;
	bool m_enabled = true;
	EnterCallback m_onEnter;
	std::unordered_set<Entity*> m_overlapping;
};
