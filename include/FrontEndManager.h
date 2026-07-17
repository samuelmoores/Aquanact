#pragma once

#include <memory>
#include "Globals.h"

class Window;
class Camera;
class FileManager;
class SceneManager;
class ProjectManager;
#include "EngineGUI.h"
#include "GameGUI.h"
#include "UICreator.h"

enum class FrontEndMode {
	EngineEditor,
	UICreator
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

	void SetMode(FrontEndMode mode);
	FrontEndMode FrontEndModeValue() const;
	void OpenUICreator();
	void ReturnToEngineEditor();

	EngineMode AppMode() const;

	bool IsEditorMode() const;
	bool IsGameMode() const;
	EngineGUI& EditorGUI();
	const EngineGUI& EditorGUI() const;
	GameGUI& RuntimeGUI();
	const GameGUI& RuntimeGUI() const;
	UICreator& Creator();
	const UICreator& Creator() const;

private:
	std::unique_ptr<EngineGUI> m_engineGUI;
	std::unique_ptr<GameGUI> m_gameGUI;
	std::unique_ptr<UICreator> m_uiCreator;
	FrontEndMode m_mode = FrontEndMode::EngineEditor;
};
