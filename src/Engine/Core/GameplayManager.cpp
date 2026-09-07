#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/Audio.h"

#include "Engine/Core/Controller.h"
#include "Engine/Core/Animator.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/PhysicsWorld.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

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
	// Publish the Main Menu input context at the same startup boundary as the
	// gameplay state. This prevents the next input update from briefly treating
	// the menu as Gameplay and hiding the mouse before the menu is rendered.
	Root::Current().InputRef().TransitionToContext(Input::InputContext::MainMenu);
	// Establish the native cursor state at the gameplay boundary as soon as the
	// session enters Main Menu. ReleaseCursorForUI hides it only when a connected
	// controller already owns input; otherwise it makes the mouse cursor visible.

	Root::Current().InputRef().ReleaseCursorForUI();

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
	Audio::StopMusic();
	m_levelManager = nullptr;
	m_state = GameState::MainMenu;
	m_cutsceneActive = false;
	m_cutsceneElapsed = 0.0f;
	m_cutsceneNextLevel.clear();
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
	EnterMainMenu(frontEndManager, debug);
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
	if (!playableLevel->MusicPath().empty())
		Audio::PlayMusic("assets/" + playableLevel->MusicPath(), true, playableLevel->MusicVolume());
	EnterGameplay(frontEndManager, debug);
	debug.LogMessage("GameplayManager::BootPlayableLevel() state=Playing Scene=" + playableLevel->Name());
	return true;
}

bool GameplayManager::StartCutscene(const std::string& cutsceneName, const std::string& levelName, FrontEndManager& frontEndManager, Debug& debug)
{
	if (!m_levelManager)
	{
		return false;
	}
	Scene* cutscene = m_levelManager->FindLevel(cutsceneName);
	Scene* nextLevel = m_levelManager->FindLevel(levelName);
	if (!cutscene || m_levelManager->SceneKindFor(cutsceneName) != SceneManager::SceneKind::Cutscene ||
		!nextLevel || m_levelManager->SceneKindFor(levelName) != SceneManager::SceneKind::Level)
	{
		return false;
	}

	m_levelManager->SetActiveLevel(cutsceneName);
	m_levelManager->startUp();
	m_levelManager->CaptureActiveLevelEditorTransforms();
	cutscene->FirstFrame();
	m_cutsceneActive = true;
	m_cutsceneElapsed = 0.0f;
	m_cutsceneNextLevel = levelName;
	EnterGameplay(frontEndManager, debug);
	debug.LogMessage("GameplayManager::StartCutscene() scene=" + cutsceneName + " next=" + levelName);
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
		if (!activeLevel->MusicPath().empty())
			Audio::PlayMusic("assets/" + activeLevel->MusicPath(), true, activeLevel->MusicVolume());
	}
	EnterGameplay(frontEndManager, debug);
	debug.LogMessage("GameplayManager::StartGameSession() state=Playing");
}

void GameplayManager::EnterMainMenu(FrontEndManager& frontEndManager, Debug& debug)
{
	m_state = GameState::MainMenu;
	Root::Current().InputRef().ReleaseCursorForUI();
	SyncRuntimeUI(frontEndManager);
	debug.LogMessage("GameplayManager state=MainMenu");
	if (frontEndManager.RuntimeGUI().HasRuntime())
	{
		// Reapply the asset for editor play and hot-reloaded menus.
		frontEndManager.RuntimeGUI().RefreshUIMode();
	}
}

void GameplayManager::EnterGameplay(FrontEndManager& frontEndManager, Debug& debug)
{
	m_state = GameState::Playing;
	Root::Current().InputRef().CaptureCursorForLevel();
	SyncRuntimeUI(frontEndManager);
	debug.LogMessage("GameplayManager state=Playing");
}

void GameplayManager::EnterPauseMenu(FrontEndManager& frontEndManager, Debug& debug)
{
	m_state = GameState::Paused;
	Root::Current().InputRef().ReleaseCursorForUI();
	SyncRuntimeUI(frontEndManager);
	debug.LogMessage("GameplayManager state=Paused");
}

void GameplayManager::LeavePauseMenu(FrontEndManager& frontEndManager, Debug& debug)
{
	EnterGameplay(frontEndManager, debug);
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
	bool animationDiagnosticsPublished = false;
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

	if (m_cutsceneActive)
	{
		PathedCamera& camera = activeLevel->CameraSystem();
		const float cutsceneDuration = std::max(0.1f, activeLevel->Cutscene().duration);
		m_cutsceneElapsed += std::max(0.0f, dt);
		camera.SetPlayerProgress(m_cutsceneElapsed / cutsceneDuration);
		for (const CutsceneAnimationTrack& track : activeLevel->Cutscene().animationTracks)
		{
			if (m_cutsceneElapsed < track.startTime)
				continue;
			Entity* entity = nullptr;
			for (const auto& object : activeLevel->Objects())
			{
				if (object && object->Id() == track.entityId)
				{
					entity = object.get();
					break;
				}
			}
			EntityStateMachine* stateMachine = entity ? entity->GetEntityState() : nullptr;
			Animator* animator = stateMachine ? stateMachine->GetAnimator() : nullptr;
			if (!stateMachine || !animator)
				continue;
			int clipIndex = -1;
			const auto& animationNames = stateMachine->AnimationNames();
			for (std::size_t index = 0; index < animationNames.size(); ++index)
			{
				if (animationNames[index] == track.animationName)
				{
					clipIndex = static_cast<int>(index);
					break;
				}
			}
			if (clipIndex >= 0)
			{
				const float localTime = std::max(0.0f, m_cutsceneElapsed - track.startTime) * std::max(0.01f, track.speed);
				animator->EvaluateClipAt(clipIndex, std::min(localTime, track.duration), track.loop && localTime <= track.duration);
			}
		}
		if (m_cutsceneElapsed >= cutsceneDuration)
		{
			const std::string nextLevel = m_cutsceneNextLevel;
			m_cutsceneActive = false;
			m_cutsceneElapsed = 0.0f;
			m_cutsceneNextLevel.clear();
			if (m_levelManager->SetActiveLevel(nextLevel))
			{
				m_levelManager->startUp();
				m_levelManager->CaptureActiveLevelEditorTransforms();
				if (Scene* transitionedLevel = m_levelManager->ActiveLevel())
				{
					transitionedLevel->FirstFrame();
					if (!transitionedLevel->MusicPath().empty())
						Audio::PlayMusic("assets/" + transitionedLevel->MusicPath(), true, transitionedLevel->MusicVolume());
				}
				debug.LogMessage("GameplayManager::StartCutscene() transitioned to " + nextLevel);
			}
			return;
		}
		// Cutscene tracks own animation pose and movement while the timeline is
		// active; gameplay state machines must not overwrite the authored pose.
		return;
	}

	// How can we move this to the debugger?
	debug.SetGameplayContext(
		activeLevel->Name(),
		activeLevel->Objects().size(),
		ControllerCount(),
		engineState.IsGameMode() ? "Game" : "Editor");


	bool animationDiagnosticsPublished = false;
	for (const auto& object : activeLevel->Objects())
	{
		if (object)
		{
			if (object->GetController())
			{
				// Only controller entities are currently classified as dynamic. Static
				// level geometry keeps the bounds captured during scene registration.
				PhysicsWorld::Instance().Update(*object);
			}

			FrameProfiler::Scope controllerScope(Root::Current().Profiler(), "Controllers");
			object->UpdateComponents(dt);
			if (object->GetController())
			{
				// Camera queries run after gameplay. Refresh again after movement so
				// the followed player and other dynamic blockers use this frame's
				// transforms instead of the previous frame's cached bounds.
				PhysicsWorld::Instance().Update(*object);
			}
			if (EntityStateMachine* animator = object->GetEntityState())
			{
				if (animationDiagnosticsPublished || animator->States().empty() || animator->CurrentState().empty())
					continue;
				std::string stateListText;
				for (const auto& state : animator->States())
				{
					const std::string animationLabel = state.animationName.empty()
						? "<none>"
						: std::filesystem::path(state.animationName).filename().string();
					stateListText += state.name + " -> animation " + animationLabel + "\n";
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
				animationDiagnosticsPublished = true;
			}
		}
	}

}

void GameplayManager::ExecuteCommand(GameplayCommand command, FrontEndManager& frontEndManager, Debug& debug)
{
	switch (command)
	{
	case GameplayCommand::Pause:
		SetPaused(true, frontEndManager, debug);
		break;
	case GameplayCommand::Resume:
		SetPaused(false, frontEndManager, debug);
		break;
	case GameplayCommand::TogglePause:
		TogglePaused(frontEndManager, debug);
		break;
	}
}

void GameplayManager::SetPaused(bool paused, FrontEndManager& frontEndManager, Debug& debug)
{
	if (paused)
	{
		if (m_state == GameState::Playing)
		{
			EnterPauseMenu(frontEndManager, debug);
		}
	}
	else if (m_state == GameState::Paused)
	{
		LeavePauseMenu(frontEndManager, debug);
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





