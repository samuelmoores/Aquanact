#pragma once

#include "Engine/LevelManager.h"

class GameplayManager {
public:
	~GameplayManager();

	void startUp(LevelManager& levelManager);
	void shutDown();
	void StartGameSession();

	void Update(float dt);
	void SetPaused(bool paused);
	void TogglePaused();
	bool IsPaused() const;
	std::size_t ControllerCount() const;

private:
	LevelManager* m_levelManager = nullptr;
	bool m_paused = false;
};
