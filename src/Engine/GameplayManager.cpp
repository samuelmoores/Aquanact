#include "Engine/GameplayManager.h"

#include "Engine/Controller.h"
#include "Engine/AnimatorComponent.h"
#include "Engine/Debug.h"
#include "Engine/Globals.h"
#include "Engine/Level.h"

#include <algorithm>
#include <utility>

GameplayManager::~GameplayManager() = default;

void GameplayManager::startUp(LevelManager& levelManager)
{
	m_levelManager = &levelManager;
	m_controllers.clear();
	m_paused = false;
	if (m_levelManager)
	{
		m_levelManager->startUp();
	}
	BindActiveLevel(m_levelManager ? m_levelManager->ActiveLevel() : nullptr);
}

void GameplayManager::shutDown()
{
	m_controllers.clear();
	m_levelManager = nullptr;
	m_boundActiveLevel = nullptr;
	m_paused = false;
}

void GameplayManager::StartGameSession()
{
	if (!m_levelManager)
	{
		return;
	}

	if (!m_levelManager->ActiveLevel())
	{
		if (!m_levelManager->Levels().empty())
		{
			m_levelManager->SetActiveLevel(m_levelManager->Levels().front()->Name());
		}
		else
		{
			m_levelManager->CreateLevel("Default");
		}
	}

	m_levelManager->startUp();
	Level* activeLevel = m_levelManager->ActiveLevel();
	if (activeLevel)
	{
		activeLevel->FirstFrame();
	}
	m_boundActiveLevel = nullptr;
	RefreshControllersFromActiveLevel();
}

void GameplayManager::RegisterController(Controller* controller)
{
	if (!controller)
	{
		return;
	}

	if (std::find(m_controllers.begin(), m_controllers.end(), controller) == m_controllers.end())
	{
		controller->SetRegistered(true);
		m_controllers.push_back(controller);
	}
}

void GameplayManager::UnregisterController(Controller* controller)
{
	const auto it = std::remove(m_controllers.begin(), m_controllers.end(), controller);
	m_controllers.erase(it, m_controllers.end());
	if (controller)
	{
		controller->SetRegistered(false);
	}
}

void GameplayManager::RefreshControllersFromActiveLevel()
{
	m_controllers.clear();
	const Level* activeLevel = m_levelManager ? m_levelManager->ActiveLevel() : nullptr;
	if (!activeLevel)
	{
		return;
	}

	for (const auto& object : activeLevel->Objects())
	{
		if (!object)
		{
			continue;
		}

		if (Controller* controller = object->GetController())
		{
			if (!controller->Owner())
			{
				controller->SetOwner(object.get());
			}
			RegisterController(controller);
		}
	}
}

void GameplayManager::BindActiveLevel(Level* activeLevel)
{
	if (m_boundActiveLevel == activeLevel)
	{
		return;
	}

	m_boundActiveLevel = activeLevel;
	m_controllers.clear();
	if (!activeLevel)
	{
		return;
	}

	for (const auto& object : activeLevel->Objects())
	{
		if (!object)
		{
			continue;
		}

		if (Controller* controller = object->GetController())
		{
			if (!controller->Owner())
			{
				controller->SetOwner(object.get());
			}
			RegisterController(controller);
		}
	}
}

void GameplayManager::Update(float dt)
{
	if (m_paused)
	{
		return;
	}

	Level* activeLevel = m_levelManager ? m_levelManager->ActiveLevel() : nullptr;
	if (!activeLevel)
	{
		return;
	}

	BindActiveLevel(activeLevel);
	gDebug.SetGameplayContext(
		activeLevel->Name(),
		activeLevel->Objects().size(),
		m_controllers.size(),
		gEngineState.IsGameMode() ? "Game" : "Editor");

	for (const auto& object : activeLevel->Objects())
	{
		if (object)
		{
			object->UpdateComponents(dt);
			if (AnimatorComponent* animator = object->GetAnimatorComponent())
			{
				std::string stateListText;
				for (const auto& state : animator->States())
				{
					stateListText += state.name + " -> clip " + std::to_string(state.clipIndex) + "\n";
				}
				gDebug.SetAnimationDiagnostics(
					animator->CurrentState(),
					animator->DesiredState(),
					animator->LastTransitionDebug(),
					animator->LastTransitionFrom(),
					animator->LastTransitionTo(),
					animator->LastTransitionLeftOperandText(),
					animator->LastTransitionComparatorText(),
					animator->LastTransitionRightOperandText(),
					animator->LastTransitionLeftValue(),
					animator->LastTransitionRightValue(),
					animator->LastTransitionPassed(),
					animator->LastResolvedTargetState(),
					animator->LastResolvedTargetClipIndex(),
					animator->LastResolvedTargetFound(),
					stateListText);
			}
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


