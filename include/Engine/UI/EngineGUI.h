#pragma once

#include "Engine/Core/EntityStateMachine.h"
#include "Engine/UI/CameraPathCreator.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>

class Window; 
class Camera; 
class FileManager; 
class SceneManager; 
class ProjectManager; 
class Entity;

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

	CameraPathCreator& CameraPath();
	const CameraPathCreator& CameraPath() const;

private:
	// Popup and window drawing helpers
	void DrawBuildGamePopup();
	void DrawAddCodeFilePopup();
	void DrawNewLevelPopup();
	void DrawInputMapWindow();
	void DrawCameraWindow();
	void DrawEntityStateMachinePopup(EntityStateMachine& entityState);

	// File and name helpers
	static std::string NormalizeGameClassName(const std::string& input);
	static std::string MakeHeaderTemplate(const std::string& className);
	static std::string MakeSourceTemplate(const std::string& className);
	static std::string NormalizeLevelName(const std::string& input);
	void CreateGameCodeFile(const std::string& className);
	void CreateEntity();
	void UpdateEntity(Entity* entity);

	// ***** game code ***********

	struct NewClassConfiguration
	{
		std::string className;
		bool attachToExistingEntity = false;
		bool createNewEntity = false;
		std::string targetEntityName;
	};

	// New code helpers
	void StartRebuild();
	void OnRebuildFinished();
	void SaveNewClassConfiguration(NewClassConfiguration& configuration);

	// ***************
	// *** Members ***
	// ***************

	// Per-entity-state UI state
	struct EntityStateMachineUiState {
		bool initialized = false;
		char transitionFromState[64] = "";
		char transitionToState[64] = "";
		char transitionFilterFromState[64] = "";
		char transitionFilterToState[64] = "";
		std::map<std::string, bool> visibleStateTransitions;
		bool showIncomingTransitions = true;
		bool showOutgoingTransitions = true;
		float transitionBlendSeconds = 0.25f;
		bool transitionWaitForCurrentStateComplete = false;
		bool addStatePopupInitialized = false;
		bool editStatePopupRequested = false;
		int editingStateIndex = -1;
		char stateEditName[64] = "";
		char stateEditAnimationName[64] = "";
		bool stateEditBlocksMovement = false;
		bool stateEditBlocksInput = false;
		bool addTransitionPopupInitialized = false;
		bool editTransitionPopupRequested = false;
		bool transitionListNeedsRefresh = false;
		int editingTransitionIndex = -1;
		std::map<std::string, bool> expandedTransitionConditions;
		std::vector<EntityStateMachine::Condition> conditions;
	};

	// Input-map selection and add-action workflow
	struct InputMapUiState {
		std::string selectedAction = "Move";
		char newActionName[64] = "";
		bool addActionPopupRequested = false;
		std::string statusMessage;
	};

	// EngineGUI runtime state
	Window* m_window = nullptr;
	bool m_initialized = false;
	unsigned int m_bootTexture = 0;
	int m_bootTextureWidth = 0;
	int m_bootTextureHeight = 0;
	int m_selectedLevelObjectIndex = -1;

	// Visibility toggles
	bool m_showAxis = true;
	bool m_showGrid = true;
	bool m_showLevelWindow = true;
	bool m_showEntityWindow = false;
	bool m_showLightingWindow = false;
	bool m_showFileExplorer = false;
	bool m_showInputMapWindow = false;
	bool m_showCameraWindow = false;
	CameraPathCreator m_cameraPathCreator;

	// Popup request flags
	bool m_buildGamePopupRequested = false;
	bool m_addCodeFilePopupRequested = false;
	bool m_newLevelPopupRequested = false;
	bool m_entityStateMachinePopupRequested = false;
	bool m_componentDeletePopupRequested = false;
	bool m_createAndBuildPopupRequested = false;
	bool m_addCodeFileCreated = false;

	// Cached input and status text
	char m_newCodeFileName[128] = "";
	char m_newLevelName[128] = "Level1";
	std::string m_addCodeFileStatusMessage;
	std::string m_newLevelStatusMessage;
	std::string m_componentTypePendingDelete;
	std::string m_createAndBuildClassName;
	int m_createAndBuildEntityIndex = 0;

	// Animator UI state cache
	std::unordered_map<EntityStateMachine*, EntityStateMachineUiState> m_entityStateUiState;
	InputMapUiState m_inputMapUi;

	// New code state
	Entity* m_pendingEntity;
	std::string m_pendingClassName;
	bool m_pendingAttachToExistingEntity;
	bool m_rebuildRequested;
	bool m_rebuildInProgress;
	bool m_rebuildSucceeded;
	bool m_buildInProgress;

};



