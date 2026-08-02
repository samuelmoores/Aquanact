#include "Engine/Core/Root.h"

#include "Engine/Core/Window.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/EventManager.h"
#include "Engine/Core/FileManager.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/FrameProfiler.h"

#include <chrono>
#include <filesystem>
#include <thread>

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
SceneManager& Root::Levels() { return *m_levelManager; }
ProjectManager& Root::Projects() { return *m_projectManager; }
GameplayManager& Root::Gameplay() { return *m_gameplayManager; }
Input& Root::InputRef() { return *m_input; }
InputManager& Root::InputActions() { return *m_inputManager; }
FrameProfiler& Root::Profiler() { return *m_profiler; }
EngineState& Root::State() { return m_engineState; }
bool& Root::GameModeDebugFlag() { return m_gameModeDebug; }
bool& Root::EditorLaunchedGameSession() { return m_editorLaunchedGameSession; }

void Root::InitializeOwnedSystems()
{
	m_window = std::make_unique<Window>();
	m_renderManager = std::make_unique<RenderManager>();
	m_frontEndManager = std::make_unique<FrontEndManager>();
	m_debug = std::make_unique<Debug>();
	m_eventManager = std::make_unique<EventManager>();
	m_fileSystem = std::make_unique<FileSystem>();
	m_fileManager = std::make_unique<FileManager>(*m_fileSystem);
	m_levelManager = std::make_unique<SceneManager>();
	m_projectManager = std::make_unique<ProjectManager>(*m_fileSystem);
	m_gameplayManager = std::make_unique<GameplayManager>();
	m_input = std::make_unique<Input>();
	m_inputManager = std::make_unique<InputManager>();
	m_profiler = std::make_unique<FrameProfiler>();
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
	m_editorLaunchedGameSession = false;

	m_window->startUp();
	m_renderManager->startUp(*m_window);
	m_frontEndManager->startUp(*m_window);
	m_input->startUp(*m_window);
	m_inputManager->startUp(*m_input);
	m_debug->startUp();
	m_fileManager->startUp();
	m_projectManager->LoadProject(DefaultProjectPath(), *m_levelManager);

	StartInitialSession();
	m_started = true;
}

void Root::StartEditorSession()
{
	m_editorLaunchedGameSession = false;
	m_frontEndManager->RuntimeGUI().SetUIMode(GameGUIManager::UIMode::MainMenu);
}

void Root::StartInitialSession()
{
	if (m_engineState.IsGameMode())
	{
		m_gameplayManager->startUp(*m_levelManager, *m_frontEndManager, *m_debug, m_engineState);
		m_gameplayManager->BootMainMenu(*m_frontEndManager, *m_debug);
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
		const auto frameStart = std::chrono::steady_clock::now();
		m_profiler->BeginFrame();
		{
			FrameProfiler::Scope scope(*m_profiler, "Input");
			m_input->Update();
			m_inputManager->Update();
		}
		{
			FrameProfiler::Scope scope(*m_profiler, "Gameplay");
			UpdateFrame(m_input->DeltaTime());
		}
		{
			FrameProfiler::Scope scope(*m_profiler, "Render");
			m_renderManager->Loop(*m_frontEndManager, *m_fileManager, *m_levelManager, *m_projectManager, *m_debug, *m_input, *m_window, m_engineState);
		}

		// Frame pacing is independent from optional profiler sampling.
		if (m_frameCapEnabled && m_targetFrameRate > 0.0)
		{
			const auto targetFrameDuration = std::chrono::duration<double>(1.0 / m_targetFrameRate);
			const auto targetFrameEnd = frameStart + targetFrameDuration;
			// Windows sleep resolution can round an 8.33 ms wait up to roughly
			// 16.6 ms. Yield in the final pacing loop instead of falling to 60 Hz.
			while (std::chrono::steady_clock::now() < targetFrameEnd)
			{
				std::this_thread::yield();
			}
		}
		m_profiler->EndFrame();
	}
}

void Root::UpdateFrame(float dt)
{
	if (m_engineState.IsGameMode())
	{
		m_gameplayManager->SyncRuntimeUI(*m_frontEndManager);
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
	if (m_renderManager)
	{
		m_renderManager->GetGameCamera().SetTarget(nullptr);
	}

	m_gameplayManager->shutDown();
	m_fileManager->shutDown();
	m_debug->shutDown();
	m_inputManager->shutDown();
	m_input->shutDown();
	m_frontEndManager->shutDown();
	m_renderManager->shutDown();
	m_window->shutDown();
	m_started = false;
}



