#include "Engine/Core/GameplayManager.h"

#include "Engine/Core/Controller.h"
#include "Engine/Core/AnimatorComponent.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FrameProfiler.h"

#include <algorithm>

GameplayManager::~GameplayManager() = default;

namespace {
	constexpr const char* kMainMenuLevelName = "MainMenu";

	Scene* FindPlayableLevel(SceneManager& SceneManager)
	{
		if (Scene* startupLevel = SceneManager.FindLevel(SceneManager.StartupLevelName()); startupLevel &&
			SceneManager.SceneKindFor(startupLevel->Name()) == SceneManager::SceneKind::Level)
		{
			return startupLevel;
		}

		for (const auto& Scene : SceneManager.Levels())
		{
			if (Scene && SceneManager.SceneKindFor(Scene->Name()) == SceneManager::SceneKind::Level)
			{
				return Scene.get();
			}
		}

		return nullptr;
	}
}

void GameplayManager::startUp(SceneManager& SceneManager, FrontEndManager& frontEndManager, Debug& debug, EngineState& engineState)
{
	m_levelManager = &SceneManager;
	m_state = GameState::MainMenu;
	m_physicsAccumulator = 0.0f;
	if (m_levelManager)
	{
		// If the runtime UI is already initialized, set it to a known boot
		// state so the engine opens on the main menu instead of leaving
		// whatever screen was active from a previous session.
		if (frontEndManager.RuntimeGUI().HasRuntime())
		{
			frontEndManager.RuntimeGUI().SetUIMode(GameGUIManager::UIMode::MainMenu);
			frontEndManager.RuntimeGUI().RefreshUIMode();
		}
	}
}

void GameplayManager::shutDown()
{
	m_levelManager = nullptr;
	m_state = GameState::MainMenu;
	m_physicsAccumulator = 0.0f;
}

void GameplayManager::BootMainMenu(FrontEndManager& frontEndManager, Debug& debug)
{
	if (!m_levelManager)
	{
		return;
	}

	debug.LogMessage("GameplayManager::BootMainMenu()");
	Scene* mainMenuLevel = m_levelManager->FindLevel(kMainMenuLevelName);
	if (!mainMenuLevel)
	{
		mainMenuLevel = m_levelManager->CreateLevel(kMainMenuLevelName);
	}
	if (mainMenuLevel)
	{
		m_levelManager->SetActiveLevel(mainMenuLevel->Name());
		m_levelManager->SetStartupLevelName(mainMenuLevel->Name());
		m_levelManager->startUp();
		m_levelManager->CaptureActiveLevelEditorTransforms();
		mainMenuLevel->FirstFrame();
	}
	m_state = GameState::MainMenu;
	if (frontEndManager.RuntimeGUI().HasRuntime())
	{
		frontEndManager.RuntimeGUI().SetUIMode(GameGUIManager::UIMode::MainMenu);
	}
}

bool GameplayManager::BootPlayableLevel(FrontEndManager& frontEndManager, Debug& debug)
{
	if (!m_levelManager)
	{
		return false;
	}

	Scene* playableLevel = FindPlayableLevel(*m_levelManager);
	if (!playableLevel)
	{
		debug.LogMessage("GameplayManager::BootPlayableLevel() failed: no non-UI Scene is available.");
		return false;
	}

	m_levelManager->SetActiveLevel(playableLevel->Name());
	m_levelManager->SetStartupLevelName(playableLevel->Name());
	m_levelManager->startUp();
	m_levelManager->CaptureActiveLevelEditorTransforms();
	playableLevel->FirstFrame();
	m_state = GameState::Playing;
	debug.LogMessage("GameplayManager::BootPlayableLevel() state=Playing Scene=" + playableLevel->Name());
	SyncRuntimeUI(frontEndManager);
	return true;
}

void GameplayManager::StartGameSession(FrontEndManager& frontEndManager, Debug& debug, EngineState& engineState)
{
	if (!m_levelManager || !engineState.IsGameMode())
	{
		debug.LogMessage("GameplayManager::StartGameSession() skipped: no Scene manager or not in game mode.");
		return;
	}

	debug.LogMessage("GameplayManager::StartGameSession() begin");
	if (!m_levelManager->ActiveLevel()) // broken boundary
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
	Scene* activeLevel = m_levelManager->ActiveLevel();
	if (activeLevel)
	{
		activeLevel->FirstFrame();
	}
	m_state = GameState::Playing;
	debug.LogMessage("GameplayManager::StartGameSession() state=Playing");
	SyncRuntimeUI(frontEndManager);
}

void GameplayManager::SyncRuntimeUI(FrontEndManager& frontEndManager) const
{
	if (!frontEndManager.RuntimeGUI().HasRuntime())
	{
		return;
	}

	switch (m_state)
	{
	case GameState::MainMenu:
		frontEndManager.RuntimeGUI().SetUIMode(GameGUIManager::UIMode::MainMenu);
		break;
	case GameState::Playing:
		frontEndManager.RuntimeGUI().SetUIMode(GameGUIManager::UIMode::GameplayHUD);
		break;
	case GameState::Paused:
		frontEndManager.RuntimeGUI().SetUIMode(GameGUIManager::UIMode::PauseMenu);
		break;
	}
}

std::size_t GameplayManager::ControllerCount() const
{
	const Scene* activeLevel = m_levelManager ? m_levelManager->ActiveLevel() : nullptr;
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

void GameplayManager::Update(float dt, FrontEndManager& frontEndManager, Debug& debug, EngineState& engineState)
{
	// if we are not playing the game we should not be in the Game Loop
	if (m_state != GameState::Playing)
	{
		return;
	}

	// why is the game loop running if there could be no active Scene?
	// how can we check this before starting the game loop?
	Scene* activeLevel = m_levelManager ? m_levelManager->ActiveLevel() : nullptr;
	if (!activeLevel)
	{
		return;
	}

	// How can we move this to the debugger?
	debug.SetGameplayContext(
		activeLevel->Name(),
		activeLevel->Objects().size(),
		ControllerCount(),
		engineState.IsGameMode() ? "Game" : "Editor");


	// Controllers use a fixed simulation step so a long render frame cannot
	// turn directly into a large movement or rotation delta.
	constexpr float fixedControllerStep = 1.0f / 120.0f;
	constexpr int maxControllerSteps = 8;
	m_physicsAccumulator += std::clamp(dt, 0.0f, 0.25f);
	int controllerSteps = 0;
	while (m_physicsAccumulator >= fixedControllerStep && controllerSteps < maxControllerSteps)
	{
		for (const auto& object : activeLevel->Objects())
		{
			if (object)
			{
				object->CapturePhysicsState();
			}
		}
		for (const auto& object : activeLevel->Objects())
		{
			if (object)
			{
				FrameProfiler::Scope controllerScope(Root::Current().Profiler(), "Controllers");
				object->UpdateControllers(fixedControllerStep);
			}
		}
		m_physicsAccumulator -= fixedControllerStep;
		++controllerSteps;
	}
	if (controllerSteps == maxControllerSteps && m_physicsAccumulator >= fixedControllerStep)
	{
		// Drop excessive backlog instead of allowing a stalled frame to create an
		// unbounded catch-up loop and freeze the application.
		m_physicsAccumulator = 0.0f;
	}

	// Animation and other non-controller components remain frame-rate driven.
	for (const auto& object : activeLevel->Objects())
	{
		if (object)
		{
			FrameProfiler::Scope animationScope(Root::Current().Profiler(), "Animation");
			object->UpdateNonControllerComponents(dt);
			if (AnimatorComponent* animator = object->GetAnimatorComponent())
			{
				std::string stateListText;
				for (const auto& state : animator->States())
				{
					stateListText += state.name + " -> clip " + std::to_string(state.clipIndex) + "\n";
				}

				// but all this debugging should be moved out of the game loop
				debug.SetAnimationDiagnostics(
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

float GameplayManager::PhysicsInterpolationAlpha() const
{
	constexpr float fixedPhysicsStep = 1.0f / 120.0f;
	return std::clamp(m_physicsAccumulator / fixedPhysicsStep, 0.0f, 1.0f);
}

void GameplayManager::SetPaused(bool paused, FrontEndManager& frontEndManager, Debug& debug)
{
	if (paused)
	{
		if (m_state == GameState::Playing)
		{
			m_state = GameState::Paused;
			debug.LogMessage("GameplayManager state=Paused");
		}
	}
	else if (m_state == GameState::Paused)
	{
		m_state = GameState::Playing;
		debug.LogMessage("GameplayManager state=Playing");
	}

	if (frontEndManager.RuntimeGUI().HasRuntime())
	{
		if (m_state == GameState::Paused)
		{
			frontEndManager.RuntimeGUI().SetUIMode(GameGUIManager::UIMode::PauseMenu);
		}
		else if (m_state == GameState::Playing)
		{
			frontEndManager.RuntimeGUI().SetUIMode(GameGUIManager::UIMode::GameplayHUD);
		}
		else if (m_state == GameState::MainMenu)
		{
			frontEndManager.RuntimeGUI().SetUIMode(GameGUIManager::UIMode::MainMenu);
		}
	}
}

void GameplayManager::TogglePaused(FrontEndManager& frontEndManager, Debug& debug)
{
	SetPaused(m_state != GameState::Paused, frontEndManager, debug);
}

bool GameplayManager::IsPaused() const
{
	return m_state == GameState::Paused;
}

GameplayManager::GameState GameplayManager::State() const
{
	return m_state;
}





