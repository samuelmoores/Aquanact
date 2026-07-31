#include "Engine/Root.h"

#include <Engine/Window.h>
#include <Engine/RenderManager.h>
#include <Engine/FrontEndManager.h>
#include <Engine/GameGUIManager.h>
#include <Engine/Debug.h>
#include <Engine/EventManager.h>
#include <Engine/FileManager.h>
#include <Engine/FileSystem.h>
#include <Engine/LevelManager.h>
#include <Engine/ProjectManager.h>
#include <Engine/GameplayManager.h>
#include <Engine/Input.h>

#include <filesystem>

Root* Root::s_current = nullptr;

namespace
{
	std::filesystem::path DefaultProjectPath()
	{
		const std::filesystem::path executableProject = Root::Current().FileSystemRef().ExecutableDirectory() / "project.aqua";
		if (std::filesystem::exists(executableProject))
		{
			return executableProject;
		}

		const std::filesystem::path packagedProject = std::filesystem::current_path() / "project.aqua";
		if (std::filesystem::exists(packagedProject))
		{
			return packagedProject;
		}

#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / "projects" / "project.aqua";
#else
		return packagedProject;
#endif
	}
}

Root::Root()
{
	s_current = this;
}

Root::~Root()
{
	if (s_current == this)
	{
		s_current = nullptr;
	}
}

Root& Root::Current()
{
	return *s_current;
}

bool Root::HasCurrent()
{
	return s_current != nullptr;
}

Window& Root::WindowRef() { return *m_window; }
RenderManager& Root::Render() { return *m_renderManager; }
FrontEndManager& Root::FrontEnd() { return *m_frontEndManager; }
Debug& Root::Debugger() { return *m_debug; }
EventManager& Root::Events() { return *m_eventManager; }
FileManager& Root::Files() { return *m_fileManager; }
FileSystem& Root::FileSystemRef() { return *m_fileSystem; }
LevelManager& Root::Levels() { return *m_levelManager; }
ProjectManager& Root::Projects() { return *m_projectManager; }
GameplayManager& Root::Gameplay() { return *m_gameplayManager; }
Input& Root::InputRef() { return *m_input; }
EngineState& Root::State() { return m_engineState; }
bool& Root::GameModeDebugFlag() { return m_gameModeDebug; }

void Root::InitializeOwnedSystems()
{
	m_window = std::make_unique<Window>();
	m_renderManager = std::make_unique<RenderManager>();
	m_frontEndManager = std::make_unique<FrontEndManager>();
	m_debug = std::make_unique<Debug>();
	m_eventManager = std::make_unique<EventManager>();
	m_fileSystem = std::make_unique<FileSystem>();
	m_fileManager = std::make_unique<FileManager>(*m_fileSystem);
	m_levelManager = std::make_unique<LevelManager>();
	m_projectManager = std::make_unique<ProjectManager>(*m_fileSystem);
	m_gameplayManager = std::make_unique<GameplayManager>();
	m_input = std::make_unique<Input>();
}

void Root::startUp(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	InitializeOwnedSystems();

#ifdef AQUANACT_GAME
	m_engineState.SetMode(EngineMode::Game);
#else
	m_engineState.SetMode(EngineMode::Editor);
#endif

	m_window->startUp();
	m_renderManager->startUp(*m_window);
	m_frontEndManager->startUp(*m_window);
	m_input->startUp(*m_window);
	m_debug->startUp();
	m_fileManager->startUp();
	m_projectManager->LoadProject(DefaultProjectPath(), *m_levelManager);

	StartInitialSession();
	m_started = true;
}

void Root::StartEditorSession()
{
	m_frontEndManager->RuntimeGUI().SetUIMode(GameGUIManager::UIMode::MainMenu);
}

void Root::StartInitialSession()
{
	if (m_engineState.IsGameMode())
	{
		m_gameplayManager->startUp(*m_levelManager, *m_frontEndManager, *m_debug, m_engineState);
		m_gameplayManager->StartGameSession(*m_frontEndManager, *m_debug, m_engineState);
	}
	else
	{
		StartEditorSession();
	}
}

void Root::run()
{
	while (!m_window->ShouldClose())
	{
		m_input->Update();
		UpdateFrame(m_input->DeltaTime());
		m_renderManager->Loop(*m_frontEndManager, *m_fileManager, *m_levelManager, *m_projectManager, *m_debug, *m_input, *m_window, m_engineState);
	}
}

void Root::UpdateFrame(float dt)
{
	if (m_engineState.IsGameMode())
	{
		switch (m_gameplayManager->State())
		{
		case GameplayManager::GameState::MainMenu:
			m_frontEndManager->RuntimeGUI().SetUIMode(GameGUIManager::UIMode::MainMenu);
			break;
		case GameplayManager::GameState::Playing:
			m_frontEndManager->RuntimeGUI().SetUIMode(GameGUIManager::UIMode::GameplayHUD);
			break;
		case GameplayManager::GameState::Paused:
			m_frontEndManager->RuntimeGUI().SetUIMode(GameGUIManager::UIMode::PauseMenu);
			break;
		}

		// are we in gameplay?
		if (m_gameplayManager->State() == GameplayManager::GameState::Playing)
		{
			m_gameplayManager->Update(dt, *m_frontEndManager, *m_debug, m_engineState);
		}
	}

	//RenderManager loop handles frontendmanager which handles editor side UI
}

void Root::shutDown()
{
	if (!m_started)
	{
		return;
	}

	m_debug->LogMessage("Main loop exiting because the window close flag was set.");

	m_gameplayManager->shutDown();
	m_fileManager->shutDown();
	m_debug->shutDown();
	m_input->shutDown();
	m_frontEndManager->shutDown();
	m_renderManager->shutDown();
	m_window->shutDown();
	m_started = false;
}
