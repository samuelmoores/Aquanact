#include "Engine/GameplayManager.h"

#include "Engine/Controller.h"
#include "Engine/AnimatorComponent.h"
#include "Engine/Debug.h"
#include "Engine/FrontEndManager.h"
#include "Engine/Globals.h"
#include "Engine/Level.h"

GameplayManager::~GameplayManager() = default;

void GameplayManager::startUp(LevelManager& levelManager)
{
	m_levelManager = &levelManager;
	m_state = FlowState::MainMenu;
	if (m_levelManager)
	{
		if (gFrontEndManager.RuntimeGUI().HasRuntime())
		{
			gFrontEndManager.RuntimeGUI().ShowMainMenu();
		}
	}
}

void GameplayManager::shutDown()
{
	m_levelManager = nullptr;
	m_state = FlowState::MainMenu;
}

void GameplayManager::BootMainMenu()
{
	if (!m_levelManager)
	{
		return;
	}

	m_levelManager->Clear();
	m_state = FlowState::MainMenu;
	if (gFrontEndManager.RuntimeGUI().HasRuntime())
	{
		gFrontEndManager.RuntimeGUI().ShowMainMenu();
	}
}

void GameplayManager::StartGameSession()
{
	if (!m_levelManager || !gEngineState.IsGameMode())
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
	m_levelManager->CaptureActiveLevelEditorTransforms();
	Level* activeLevel = m_levelManager->ActiveLevel();
	if (activeLevel)
	{
		activeLevel->FirstFrame();
	}
	m_state = FlowState::Playing;
	if (gFrontEndManager.RuntimeGUI().HasRuntime())
	{
		gFrontEndManager.RuntimeGUI().ShowGameplayHUD();
	}
}

void GameplayManager::StartLevelPreview()
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
	m_levelManager->CaptureActiveLevelEditorTransforms();
	Level* activeLevel = m_levelManager->ActiveLevel();
	if (activeLevel)
	{
		activeLevel->FirstFrame();
	}
	m_state = FlowState::Preview;
}

std::size_t GameplayManager::ControllerCount() const
{
	const Level* activeLevel = m_levelManager ? m_levelManager->ActiveLevel() : nullptr;
	if (!activeLevel)
	{
		return 0;
	}

	std::size_t count = 0;
	for (const auto& object : activeLevel->Objects())
	{
		if (object && object->GetController())
		{
			++count;
		}
	}
	return count;
}

void GameplayManager::Update(float dt)
{
	if (m_state != FlowState::Playing && m_state != FlowState::Preview)
	{
		return;
	}

	Level* activeLevel = m_levelManager ? m_levelManager->ActiveLevel() : nullptr;
	if (!activeLevel)
	{
		return;
	}

	gDebug.SetGameplayContext(
		activeLevel->Name(),
		activeLevel->Objects().size(),
		ControllerCount(),
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

}

void GameplayManager::SetPaused(bool paused)
{
	if (paused)
	{
		if (m_state == FlowState::Playing)
		{
			m_state = FlowState::Paused;
		}
	}
	else if (m_state == FlowState::Paused)
	{
		m_state = FlowState::Playing;
	}

	if (gFrontEndManager.RuntimeGUI().HasRuntime())
	{
		if (m_state == FlowState::Paused)
		{
			gFrontEndManager.RuntimeGUI().ShowPauseMenu();
		}
		else if (m_state == FlowState::Playing)
		{
			gFrontEndManager.RuntimeGUI().ShowGameplayHUD();
		}
		else if (m_state == FlowState::MainMenu)
		{
			gFrontEndManager.RuntimeGUI().ShowMainMenu();
		}
	}
}

void GameplayManager::TogglePaused()
{
	SetPaused(m_state != FlowState::Paused);
}

bool GameplayManager::IsPaused() const
{
	return m_state == FlowState::Paused;
}

GameplayManager::FlowState GameplayManager::State() const
{
	return m_state;
}

bool GameplayManager::ShouldUpdateInEditor() const
{
	return m_state == FlowState::Preview;
}


