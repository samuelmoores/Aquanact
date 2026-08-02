#pragma once

#include "Engine/Core/AnimatorComponent.h"
#include <string>
#include <unordered_map>
class Window; class Camera; class FileManager; class SceneManager; class ProjectManager; class Entity;
class EngineGUI {
public:
	EngineGUI() = default; void startUp(Window& window); void shutDown(); void BeginFrame(); void Draw(const Camera& camera, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager); void EndFrame();
	bool ShowAxis() const; bool ShowGrid() const; bool ShowLevelWindow() const; bool ShowEntityWindow() const; bool ShowLightingWindow() const; bool ShowFileExplorer() const; bool ShowInputMapWindow() const; bool ShowCameraWindow() const; void SetShowAxis(bool showAxis); void SetShowGrid(bool showGrid); void SetShowLevelWindow(bool showLevelWindow); void SetShowEntityWindow(bool showEntityWindow); void SetShowLightingWindow(bool showLightingWindow); void SetShowFileExplorer(bool showFileExplorer); void SetShowInputMapWindow(bool showInputMapWindow); void SetShowCameraWindow(bool showCameraWindow);
private:
	struct AnimatorStateMachineUiState { bool initialized = false; char initialStateName[64] = ""; char transitionFromState[64] = ""; char transitionToState[64] = ""; float transitionBlendSeconds = 0.25f; bool addTransitionPopupInitialized = false; int editingTransitionIndex = -1; std::vector<AnimatorComponent::Condition> conditions; };
	void DrawBuildGamePopup(); void DrawAddCodeFilePopup(); void DrawNewLevelPopup(); void DrawInputMapWindow(); void DrawCameraWindow(); void DrawAnimatorStateMachinePopup(AnimatorComponent& animator); static std::string NormalizeGameClassName(const std::string& input); static std::string MakeHeaderTemplate(const std::string& className); static std::string MakeSourceTemplate(const std::string& className); static std::string NormalizeLevelName(const std::string& input); void CreateGameCodeFile(const std::string& className);
	Window* m_window = nullptr; bool m_showAxis = true; bool m_showGrid = true; bool m_initialized = false; unsigned int m_bootTexture = 0; int m_bootTextureWidth = 0; int m_bootTextureHeight = 0; int m_selectedLevelObjectIndex = -1; bool m_showLevelWindow = true; bool m_showEntityWindow = false; bool m_showLightingWindow = false; bool m_showFileExplorer = false; bool m_showInputMapWindow = false; bool m_showCameraWindow = false; bool m_buildGamePopupRequested = false; bool m_addCodeFilePopupRequested = false; bool m_newLevelPopupRequested = false; bool m_animatorStateMachinePopupRequested = false; bool m_addCodeFileCreated = false; char m_newCodeFileName[128] = "PlayerHealth"; char m_newLevelName[128] = "Level1"; std::string m_addCodeFileStatusMessage; std::string m_newLevelStatusMessage; std::unordered_map<AnimatorComponent*, AnimatorStateMachineUiState> m_animatorUiState;
};


