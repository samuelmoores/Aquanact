#pragma once

class Camera;
class FileManager;
class ProjectManager;
class SceneManager;
class Window;
class CameraPathCreator;

struct EngineGuiSelection
{
	// Entity IDs remain stable when the scene vector is reordered or an earlier
	// object is removed. Zero is reserved for no selection.
	unsigned int entityId = 0;
	// Point lights are stored in a fixed-capacity vector and are currently only
	// appended, so their index is sufficient for editor selection. -1 is none.
	int pointLightIndex = -1;
	// Index into the active scene's independent level-collider collection.
	int levelColliderIndex = -1;
};

// Visibility owned by EngineGUI and shared with the menu and window dispatch.
struct EngineGuiWindowState
{
	bool showSceneWindow = true;
	bool showLevelColliderWindow = false;
	bool showEntityWindow = false;
	bool showLightingWindow = false;
	bool showFileExplorer = false;
	bool showInputMapWindow = false;
	bool showCameraWindow = false;
	bool showAudioWindow = false;
	bool showCutsceneWindow = true;
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
	bool* gizmoTranslate = nullptr;
	bool* gizmoRotate = nullptr;
	bool* gizmoScale = nullptr;
	bool* boxFaceDragMode = nullptr;
	CameraPathCreator* cameraPath = nullptr;
	bool* showCameraPath = nullptr;
};
