#pragma once

#include <memory>

class Window;
class RenderManager;
class FrontEndManager;
class Debug;
class FileManager;
class FileSystem;
class EventManager;
class SceneManager;
class ProjectManager;
class GameplayManager;
class Input;
class InputManager;
class FrameProfiler;

enum class EngineMode {
	Editor,
	Game
};

class EngineState {
public:
	void SetMode(EngineMode mode) { m_mode = mode; }
	EngineMode Mode() const { return m_mode; }
	bool IsEditorMode() const { return m_mode == EngineMode::Editor; }
	bool IsGameMode() const { return m_mode == EngineMode::Game; }

private:
	EngineMode m_mode = EngineMode::Editor;
};

class Root
{
public:
	Root();
	~Root();

	void startUp(int argc, char** argv);
	void run();
	void shutDown();

	static Root& Current();
	static bool HasCurrent();

	Window& WindowRef();
	RenderManager& Render();
	FrontEndManager& FrontEnd();
	Debug& Debugger();
	EventManager& Events();
	FileManager& Files();
	FileSystem& FileSystemRef();
	SceneManager& Levels();
	ProjectManager& Projects();
	GameplayManager& Gameplay();
	Input& InputRef();
	InputManager& InputActions();
	FrameProfiler& Profiler();
	EngineState& State();
	bool& GameModeDebugFlag();
	bool& EditorLaunchedGameSession();

private:
	void StartEditorSession();
	void StartInitialSession();
	void UpdateFrame(float dt);
	void InitializeOwnedSystems();

	std::unique_ptr<Window> m_window;
	std::unique_ptr<RenderManager> m_renderManager;
	std::unique_ptr<FrontEndManager> m_frontEndManager;
	std::unique_ptr<Debug> m_debug;
	std::unique_ptr<EventManager> m_eventManager;
	std::unique_ptr<FileSystem> m_fileSystem;
	std::unique_ptr<FileManager> m_fileManager;
	std::unique_ptr<SceneManager> m_levelManager;
	std::unique_ptr<ProjectManager> m_projectManager;
	std::unique_ptr<GameplayManager> m_gameplayManager;
	std::unique_ptr<Input> m_input;
	std::unique_ptr<InputManager> m_inputManager;
	std::unique_ptr<FrameProfiler> m_profiler;
	bool m_frameCapEnabled = true;
	double m_targetFrameRate = 120.0;
	EngineState m_engineState;
	bool m_gameModeDebug = true;
	bool m_editorLaunchedGameSession = false;
	bool m_started = false;

	static Root* s_current;
};


