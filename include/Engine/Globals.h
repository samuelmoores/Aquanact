#pragma once

#include <memory>

class Window;
class RenderManager;
class FrontEndManager;
class Debug;
class FileManager;
class FileSystem;
class EventManager;
class LevelManager;
class ProjectManager;
class GameplayManager;
class Input;

enum class EngineMode {
	Editor,
	Game
};

// Set to true to show the ImGui Game Input and GameGUI diagnostics windows
// while the application is running in GameMode.
inline bool GameModeDebug = true;

class EngineState {
public:
	void SetMode(EngineMode mode) { m_mode = mode; }
	EngineMode Mode() const { return m_mode; }
	bool IsEditorMode() const { return m_mode == EngineMode::Editor; }
	bool IsGameMode() const { return m_mode == EngineMode::Game; }

private:
	EngineMode m_mode = EngineMode::Editor;
};

extern Window gWindow;
extern RenderManager gRenderManager;
extern FrontEndManager gFrontEndManager;
extern Debug gDebug;
extern EventManager gEventManager;
extern FileManager gFileManager;
extern FileSystem gFileSystem;
extern LevelManager gLevelManager;
extern ProjectManager gProjectManager;
extern GameplayManager gGameplayManager;
extern Input gInput;
extern EngineState gEngineState;
