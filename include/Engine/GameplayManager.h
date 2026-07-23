#pragma once

#include "Engine/EventManager.h"
#include "Engine/LevelManager.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <memory>
#include <vector>

class Controller;

class GameplayManager {
public:
	~GameplayManager();

	void startUp(LevelManager& levelManager, Level* activeLevel = nullptr);
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

	EventManager& Events() { return m_eventManager; }
	LevelManager& Levels() { return *m_levelManager; }
	const LevelManager& Levels() const { return *m_levelManager; }

private:
	void BindActiveLevel(const Level* activeLevel);
	std::vector<Controller*> m_controllers;
	EventManager m_eventManager;
	LevelManager* m_levelManager = nullptr;
	const Level* m_boundActiveLevel = nullptr;
	std::unique_ptr<PlayerHealth> m_demoPlayerHealth;
	std::unique_ptr<Enemy> m_demoEnemy;
	bool m_paused = false;
};
