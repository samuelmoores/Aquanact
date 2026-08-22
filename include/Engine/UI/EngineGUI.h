#pragma once

#include "Engine/UI/CameraPathCreator.h"
#include "Engine/UI/EngineGuiContext.h"
#include "Engine/UI/EngineMenuBar.h"
#include "Engine/UI/SceneWindow.h"
#include "Engine/UI/EntityWindow.h"
#include "Engine/UI/EntityStateMachineWindow.h"
#include "Engine/UI/InputMapWindow.h"
#include "Engine/UI/CodeCreationWindow.h"
#include "Engine/UI/ComponentDeletionWindow.h"
#include "Engine/UI/BuildGameWindow.h"
#include "Engine/UI/AudioWindow.h"
#include "Engine/UI/CameraWindow.h"
#include "Engine/UI/LightingWindow.h"
#include "Engine/UI/FileExplorerWindow.h"
#include "Engine/UI/EditorSceneInteraction.h"

class Window; 
class Camera; 
class FileManager; 
class SceneManager; 
class ProjectManager; 

class EngineGUI {
public:
	// Lifecycle
	EngineGUI() = default;
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw(const Camera& camera, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager);
	void EndFrame();

	// Visibility flags
	bool ShowAxis() const;
	bool ShowGrid() const;
	bool ShowLevelWindow() const;
	bool ShowEntityWindow() const;
	bool ShowLightingWindow() const;
	bool ShowFileExplorer() const;
	bool ShowInputMapWindow() const;
	bool ShowCameraWindow() const;
	void SetShowAxis(bool showAxis);
	void SetShowGrid(bool showGrid);
	void SetShowLevelWindow(bool showLevelWindow);
	void SetShowEntityWindow(bool showEntityWindow);
	void SetShowLightingWindow(bool showLightingWindow);
	void SetShowFileExplorer(bool showFileExplorer);
	void SetShowInputMapWindow(bool showInputMapWindow);
	void SetShowCameraWindow(bool showCameraWindow);
	bool ShowCameraPath() const;
	void SetShowCameraPath(bool showCameraPath);

	CameraPathCreator& CameraPath();
	const CameraPathCreator& CameraPath() const;
	unsigned int SelectedEntityId() const { return m_selection.entityId; }

private:
	// EngineGUI runtime state
	Window* m_window = nullptr;
	bool m_initialized = false;
	unsigned int m_bootTexture = 0;
	int m_bootTextureWidth = 0;
	int m_bootTextureHeight = 0;
	EngineGuiSelection m_selection;
	bool m_gizmoTranslate = true;
	bool m_gizmoRotate = false;
	bool m_gizmoScale = false;
	bool m_boxFaceDragMode = false;
	EngineMenuBar m_menuBar;
	SceneWindow m_sceneWindow;
	EntityWindow m_entityWindow;
	EditorSceneInteraction m_sceneInteraction;

	// Visibility toggles
	bool m_showAxis = true;
	bool m_showGrid = true;
	EngineGuiWindowState m_windowState;
	bool m_showCameraPath = false;
	CameraPathCreator m_cameraPathCreator;

	EngineGuiPopupRequests m_popupRequests;

	// Animator UI state cache
	EntityStateMachineWindow m_stateMachineWindow;
	InputMapWindow m_inputMapWindow;
	CodeCreationWindow m_codeCreationWindow;
	ComponentDeletionWindow m_componentDeletionWindow;
	BuildGameWindow m_buildGameWindow;
	AudioWindow m_audioWindow;
	CameraWindow m_cameraWindow;
	LightingWindow m_lightingWindow;
	FileExplorerWindow m_fileExplorerWindow;
};



