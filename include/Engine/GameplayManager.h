#pragma once

#include "Engine/EventManager.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <memory>
#include <vector>

class Controller;

class GameplayManager {
public:
	~GameplayManager();

	void startUp();
	void shutDown();

	void RegisterController(Controller* controller);
	void UnregisterController(Controller* controller);
	void Update(float dt);
	void SetPaused(bool paused);
	void TogglePaused();
	bool IsPaused() const;

	EventManager& Events() { return m_eventManager; }

private:
	std::vector<Controller*> m_controllers;
	EventManager m_eventManager;
	std::unique_ptr<PlayerHealth> m_demoPlayerHealth;
	std::unique_ptr<Enemy> m_demoEnemy;
	bool m_paused = false;
};
