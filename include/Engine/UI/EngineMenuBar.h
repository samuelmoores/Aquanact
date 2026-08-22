#pragma once

#include "Engine/UI/EngineGuiContext.h"

#include <optional>

class FileManager;
class ProjectManager;
class SceneManager;
class Window;

struct EngineMenuBarResult
{
	std::optional<bool> showFileExplorer;
	std::optional<bool> showSceneWindow;
	std::optional<bool> showEntityWindow;
	std::optional<bool> showLightingWindow;
};

class EngineMenuBar
{
public:
	EngineMenuBarResult Draw(
		const EngineGuiFrameContext& context,
		bool& showAxis,
		bool& showGrid,
		EngineGuiWindowState& windowState,
		EngineGuiPopupRequests& popupRequests) const;

private:
	EngineMenuBarResult DrawViewMenu(
		bool& showAxis,
		bool& showGrid,
		EngineGuiWindowState& windowState) const;
	void DrawLightingMenu() const;
	void DrawAquanactMenu(Window* window, bool& showInputMap, bool& buildGameRequested) const;
	void DrawFileMenu(SceneManager& sceneManager, ProjectManager& projectManager) const;
	void DrawSceneMenu(SceneManager& sceneManager, bool& newLevelRequested) const;
	void DrawCodeMenu(bool& addCodeFileRequested, bool& deleteComponentRequested) const;
	void DrawUiMenu() const;
	void DrawGameMenu(ProjectManager& projectManager, SceneManager& sceneManager) const;
};
