#include "GameplayManager.h"

#include "Controller.h"

#include <algorithm>

void GameplayManager::startUp()
{
	m_controllers.clear();
}

void GameplayManager::shutDown()
{
	m_controllers.clear();
}

void GameplayManager::RegisterController(Controller* controller)
{
	if (!controller)
	{
		return;
	}

	if (std::find(m_controllers.begin(), m_controllers.end(), controller) == m_controllers.end())
	{
		m_controllers.push_back(controller);
	}
}

void GameplayManager::UnregisterController(Controller* controller)
{
	const auto it = std::remove(m_controllers.begin(), m_controllers.end(), controller);
	m_controllers.erase(it, m_controllers.end());
}

void GameplayManager::Update(float dt)
{
	for (Controller* controller : m_controllers)
	{
		if (controller && controller->Owner())
		{
			controller->Update(*controller->Owner(), dt);
		}
	}
}
