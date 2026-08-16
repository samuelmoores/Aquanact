#pragma once

class Camera;
class FileManager;
class ProjectManager;
class SceneManager;
class Window;

struct EngineGuiSelection
{
	int entityIndex = -1;
};

// Visibility owned by EngineGUI and shared with the menu and window dispatch.
struct EngineGuiWindowState
{
	bool showSceneWindow = true;
	bool showEntityWindow = false;
	bool showLightingWindow = false;
	bool showFileExplorer = false;
	bool showInputMapWindow = false;
	bool showCameraWindow = false;
	bool showAudioWindow = false;
};

// One-frame requests emitted by the menu and consumed by popup owners.
struct EngineGuiPopupRequests
{
	bool buildGame = false;
	bool addCodeFile = false;
	bool newScene = false;
	bool deleteComponent = false;
};

// Non-owning dependencies valid for the duration of one EngineGUI frame.
struct EngineGuiFrameContext
{
	Window* window = nullptr;
	const Camera* camera = nullptr;
	FileManager* fileManager = nullptr;
	SceneManager* sceneManager = nullptr;
	ProjectManager* projectManager = nullptr;
	EngineGuiSelection* selection = nullptr;
};
