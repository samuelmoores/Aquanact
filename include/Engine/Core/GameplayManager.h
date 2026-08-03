#pragma once

#include "Engine/Core/SceneManager.h"

class FrontEndManager;
class Debug;
class EngineState;

class GameplayManager {
public:
	enum class GameState
	{
		MainMenu,
		Playing,
		Paused
	};

	~GameplayManager();

	void startUp(SceneManager& levelManager, FrontEndManager& frontEndManager, Debug& debug, EngineState& engineState);
	void shutDown();
	void BootMainMenu(FrontEndManager& frontEndManager, Debug& debug);
	bool BootPlayableLevel(FrontEndManager& frontEndManager, Debug& debug);
	void StartGameSession(FrontEndManager& frontEndManager, Debug& debug, EngineState& engineState);
	void SyncRuntimeUI(FrontEndManager& frontEndManager) const;

	void Update(float dt, FrontEndManager& frontEndManager, Debug& debug, EngineState& engineState);
	void SetPaused(bool paused, FrontEndManager& frontEndManager, Debug& debug);
	void TogglePaused(FrontEndManager& frontEndManager, Debug& debug);
	bool IsPaused() const;
	GameState State() const;
	std::size_t ControllerCount() const;
	float PhysicsInterpolationAlpha() const;

private:
	SceneManager* m_levelManager = nullptr;
	GameState m_state = GameState::MainMenu;
	float m_physicsAccumulator = 0.0f;
};

