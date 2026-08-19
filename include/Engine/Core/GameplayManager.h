#pragma once

#include "Engine/Core/SceneManager.h"

class FrontEndManager;
class Debug;
class EngineState;

enum class GameplayCommand
{
	Pause,
	Resume,
	TogglePause,
};

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
	void ExecuteCommand(GameplayCommand command, FrontEndManager& frontEndManager, Debug& debug);
	void SetPaused(bool paused, FrontEndManager& frontEndManager, Debug& debug);
	void TogglePaused(FrontEndManager& frontEndManager, Debug& debug);
	bool IsPaused() const;
	GameState State() const;
	std::size_t ControllerCount() const;

private:
	void EnterMainMenu(FrontEndManager& frontEndManager, Debug& debug);
	void EnterGameplay(FrontEndManager& frontEndManager, Debug& debug);
	void EnterPauseMenu(FrontEndManager& frontEndManager, Debug& debug);
	void LeavePauseMenu(FrontEndManager& frontEndManager, Debug& debug);

	SceneManager* m_levelManager = nullptr;
	GameState m_state = GameState::MainMenu;
};

