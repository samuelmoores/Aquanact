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
#include "Engine/Core/UIInputRouter.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/Audio.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/ComponentRegistry.h"

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

#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / "projects" / "project.aqua";
#else
		return executableProject;
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
SceneManager& Root::Scenes() { return *m_sceneManager; }
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
	m_sceneManager = std::make_unique<SceneManager>();
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

	// window
	m_window->startUp();

	// TODO: have window set this in its startUp
	m_targetFrameRate = m_window->RefreshRate();

	//TODO: make an audiomanager and call its startUp function here
	Audio::Init();

	// render -> frontend -> input -> input -> debug -> files
	m_renderManager->startUp(*m_window);
	m_frontEndManager->startUp(*m_window);
	m_input->startUp(*m_window);
	m_inputManager->startUp(*m_input);
	m_debug->startUp();
	m_fileManager->startUp();

	// TODO: put this into a startUp function for projectmanager
	RegisterGameComponents();
	const std::filesystem::path projectPath = DefaultProjectPath();
	m_projectManager->LoadProject(projectPath, *m_sceneManager);

	// scene
	m_sceneManager->startUp();

	// TODO: put this into scenemanager
	if (m_sceneManager->AppliedNewClassConfigurationOnStartup())
	{
		m_projectManager->SaveProject(projectPath, *m_sceneManager);
	}

	// launch game or editor
	if (m_engineState.IsGameMode())
	{
		m_gameplayManager->startUp(*m_sceneManager, *m_frontEndManager, *m_debug, m_engineState);
		m_gameplayManager->BootMainMenu(*m_frontEndManager, *m_debug);
	}
	else
	{
		m_editorLaunchedGameSession = false;
		m_frontEndManager->RuntimeGUI().SetUIMode(GameGUIManager::UIMode::MainMenu);
	}

	m_started = true;
}

void Root::run()
{
	while (!m_window->ShouldClose())
	{
		const auto frameStart = std::chrono::steady_clock::now();
		m_profiler->BeginFrame();

		// input
		{
			FrameProfiler::Scope scope(*m_profiler, "Input");

			// Deliver GLFW callbacks before building this frame's input snapshot.
			// This keeps UI hover/click routing and gameplay actions on the same
			// event batch instead of making input one frame behind rendering.
			m_window->PollEvents();

			const UIInputRoute inputRoute = UIInputRouter::Resolve(
				m_engineState.IsGameMode(),
				m_gameplayManager->State(),
				m_frontEndManager->FrontEndModeValue());

			m_input->TransitionToContext(inputRoute.context);

			// Input uses the same frame-level ownership decision for gameplay look.
			// This keeps low-level input independent from ImGui's global state.
			m_input->SetMouseCapturedByUI(inputRoute.captureMask.mouseButtons);
			m_input->Update();

			// MyGUI receives mouse events only when the frame router selects it.
			// Menus and the GameGUI preview are MyGUI-owned; an ImGui-owned mouse
			// interaction takes precedence over that route.
			m_input->DispatchPendingMouseEvents(inputRoute.dispatchMouseToMyGUI);
			m_inputManager->SetCaptureMask(inputRoute.captureMask);
			m_inputManager->Update();

			const bool debugWindowsToggleDown = m_input->KeyDown(GLFW_KEY_F1);
			if (debugWindowsToggleDown && !m_previousDebugWindowsToggle)
			{
				const bool showRuntimeDebugWindow = !m_frontEndManager->RuntimeGUI().ShowRuntimeDebugWindow();
				m_frontEndManager->RuntimeGUI().SetShowRuntimeDebugWindow(showRuntimeDebugWindow);
				m_input->RevealCursorForFrame();
			}
			m_previousDebugWindowsToggle = debugWindowsToggleDown;
		}

		// gameplay
		if (m_engineState.IsGameMode())
		{
			FrameProfiler::Scope scope(*m_profiler, "Gameplay");

			// The Pause input is a toggle. Evaluate it before the gameplay update so
			// the transition takes effect for this frame in either Playing or Paused.
			if (m_inputManager->WasPressed("Pause"))
			{
				m_gameplayManager->ExecuteCommand(
					GameplayCommand::TogglePause,
					*m_frontEndManager,
					*m_debug);
			}

			// main menu, HUD or pause menu
			m_gameplayManager->SyncRuntimeUI(*m_frontEndManager);

			// are we playing?
			if (m_gameplayManager->State() == GameplayManager::GameState::Playing)
			{
				m_gameplayManager->Update(m_input->Frame().deltaTime, *m_frontEndManager, *m_debug, m_engineState);
			}
		}

		// render
		{
			FrameProfiler::Scope scope(*m_profiler, "Render");
			m_renderManager->Loop(*m_frontEndManager, *m_fileManager, *m_sceneManager, *m_projectManager, *m_debug, *m_input, *m_window, m_engineState);
		}

		// Make the displayed frame time include outstanding GPU work. Without this,
		// the CPU can report 120 FPS while the GPU is still presenting frames much
		// more slowly. This is intentionally only enabled when the profiler is on.
		if (m_profiler->IsEnabled())
		{
			glFinish();
		}

		// Frame pacing is independent from optional profiler sampling.
		if (m_frameCapEnabled && m_targetFrameRate > 0.0)
		{
			const auto targetFrameDuration = std::chrono::duration<double>(1.0 / m_targetFrameRate);
			const auto targetFrameEnd = frameStart + targetFrameDuration;
			// Windows sleep calls may overshoot an 8.33 ms frame and effectively
			// turn a 120 Hz cap into 60 Hz. Yield until the precise deadline.
			while (std::chrono::steady_clock::now() < targetFrameEnd)
			{
				std::this_thread::yield();
			}
		}
		m_profiler->EndFrame();
	}
}

void Root::shutDown()
{
	if (!m_started)
	{
		return;
	}

	m_gameplayManager->shutDown();
	Audio::Shutdown();
	m_fileManager->shutDown();
	m_debug->shutDown();
	m_inputManager->shutDown();
	m_input->shutDown();
	m_frontEndManager->shutDown();
	m_renderManager->shutDown();
	m_window->shutDown();
	m_started = false;
}



