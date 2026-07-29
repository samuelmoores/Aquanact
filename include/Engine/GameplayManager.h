#pragma once

#include "Engine/LevelManager.h"

class GameplayManager {
public:
	enum class FlowState
	{
		MainMenu,
		Playing,
		Paused,
		Preview
	};

	~GameplayManager();

	void startUp(LevelManager& levelManager);
	void shutDown();
	void BootMainMenu();
	void StartGameSession();
	void StartLevelPreview();

	void Update(float dt);
	void SetPaused(bool paused);
	void TogglePaused();
	bool IsPaused() const;
	FlowState State() const;
	bool ShouldUpdateInEditor() const;
	std::size_t ControllerCount() const;

private:
	LevelManager* m_levelManager = nullptr;
	FlowState m_state = FlowState::MainMenu;
};
