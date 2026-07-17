#include "GameplayManager.h"

#include "Controller.h"
#include "Globals.h"
#include "SceneManager.h"
#include "Object3D.h"

#include <algorithm>

void GameplayManager::startUp()
{
	m_controllers.clear();
	m_paused = false;
}

void GameplayManager::shutDown()
{
	m_controllers.clear();
	m_paused = false;
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
	if (m_paused)
	{
		return;
	}

	for (const auto& object : gSceneManager.Objects())
	{
		if (object)
		{
			object->UpdateComponents(dt);
		}
	}

	for (Controller* controller : m_controllers)
	{
		if (controller && controller->Owner())
		{
			controller->Update(*controller->Owner(), dt);
		}
	}
}

void GameplayManager::SetPaused(bool paused)
{
	m_paused = paused;
}

void GameplayManager::TogglePaused()
{
	m_paused = !m_paused;
}

bool GameplayManager::IsPaused() const
{
	return m_paused;
}
