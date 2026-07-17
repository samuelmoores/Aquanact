#pragma once

#include <memory>
#include "Globals.h"

class Window;
class Camera;
class FileManager;
class SceneManager;
class ProjectManager;
#include "EngineGUI.h"
#include "GameGUIManager.h"
#include "GameGUICreator.h"

enum class FrontEndMode {
	EngineEditor,
	GameGUICreator
};

class FrontEndManager {
public:
	FrontEndManager() = default;
	~FrontEndManager();

	void startUp(Window& window);
	void shutDown();

	void BeginFrame();
	void Draw(const Camera& camera, FileManager& fileManager, SceneManager& sceneManager, ProjectManager& projectManager);
	void EndFrame();

	// FrontEndMode only controls which editor surface is visible; EngineState
	// controls whether the program is in editor or gameplay execution.
	void SetMode(FrontEndMode mode);
	FrontEndMode FrontEndModeValue() const;
	void OpenGameGUICreator();
	void ReturnToEngineGUIEditor();

	// This mirrors the global engine state so the frontend can branch cleanly
	// without coupling editor UI selection to gameplay execution state.
	EngineMode AppMode() const;

	bool IsEditorMode() const;
	bool IsGameMode() const;
	EngineGUI& EditorGUI();
	const EngineGUI& EditorGUI() const;
	GameGUIManager& RuntimeGUI();
	const GameGUIManager& RuntimeGUI() const;
	GameGUICreator& Creator();
	const GameGUICreator& Creator() const;

private:
	std::unique_ptr<EngineGUI> m_engineGUI;
	std::unique_ptr<GameGUIManager> m_gameGUI;
	std::unique_ptr<GameGUICreator> m_uiCreator;
	FrontEndMode m_mode = FrontEndMode::EngineEditor;
};
