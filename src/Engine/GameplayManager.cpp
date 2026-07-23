#include "Engine/GameplayManager.h"

#include "Engine/Controller.h"
#include "Engine/EventManager.h"
#include "Engine/Globals.h"
#include "Engine/SceneManager.h"
#include "Engine/Object3D.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <algorithm>
#include <iostream>
#include <utility>

GameplayManager::~GameplayManager() = default;

void GameplayManager::startUp()
{
	m_controllers.clear();
	m_eventManager.Clear();
	m_demoPlayerHealth = std::make_unique<PlayerHealth>();
	m_demoEnemy = std::make_unique<Enemy>();
	m_demoPlayerHealth->SubscribeToDamage(m_eventManager);
	m_demoEnemy->SubscribeToStart(m_eventManager);
	std::cout << "Initial PlayerHealth value: " << m_demoPlayerHealth->Health() << "\n";
	m_eventManager.DispatchStart();
	std::cout << "PlayerHealth value after Start dispatch: " << m_demoPlayerHealth->Health() << "\n";
	m_paused = false;
}

void GameplayManager::shutDown()
{
	m_controllers.clear();
	m_demoEnemy.reset();
	m_demoPlayerHealth.reset();
	m_eventManager.Clear();
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


