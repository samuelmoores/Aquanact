#pragma once

#include "Engine/LevelManager.h"

#include <memory>
#include <vector>

class Controller;

class GameplayManager {
public:
	~GameplayManager();

	void startUp(LevelManager& levelManager);
	void shutDown();
	void StartGameSession();

	void RegisterController(Controller* controller);
	void UnregisterController(Controller* controller);
	void RefreshControllersFromActiveLevel();
	void Update(float dt);
	void SetPaused(bool paused);
	void TogglePaused();
	bool IsPaused() const;
	std::size_t ControllerCount() const { return m_controllers.size(); }

private:
	void BindActiveLevel(Level* activeLevel);
	std::vector<Controller*> m_controllers;
	LevelManager* m_levelManager = nullptr;
	Level* m_boundActiveLevel = nullptr;
	bool m_paused = false;
};
