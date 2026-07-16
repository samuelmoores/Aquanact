#pragma once

#include <vector>

class Controller;

class GameplayManager {
public:
	void startUp();
	void shutDown();

	void RegisterController(Controller* controller);
	void UnregisterController(Controller* controller);
	void Update(float dt);

private:
	std::vector<Controller*> m_controllers;
};
