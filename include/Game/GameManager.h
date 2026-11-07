#pragma once

#include "Engine/Core/Component.h"

class Entity;

// Tracks session-wide gameplay statistics for one scene's GameManager entity.
// PlayerController reports trigger entries; GameGUI can bind to the values.
class GameManager final : public Component
{
public:
	const char* Name() const override { return "GameManager"; }
	void startUp(Entity& owner) override;
	void Update(Entity& owner, float dt) override;
	void OnTriggerEnter(Entity& triggerOwner) override;

	int OverlapCount() const { return m_overlapCount; }
	float ElapsedSeconds() const { return m_elapsedSeconds; }
	void RecordOverlap();

	#define GAME_MANAGER_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
		BIND_FUNCTION(OverlapCount) \
		BIND_FUNCTION(ElapsedSeconds)
	AQUA_DECLARE_BINDABLES(GAME_MANAGER_BINDABLES)
	#undef GAME_MANAGER_BINDABLES

private:
	int m_overlapCount = 0;
	float m_elapsedSeconds = 0.0f;
};
