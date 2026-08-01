#pragma once

#include "Engine/UI/EngineGUI.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/UI/GameGUICreator.h"

#include <memory>
#include <string>
#include <vector>

class Window;
class Camera;
class FileManager;
class LevelManager;
class ProjectManager;
enum class EngineMode;

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
	void DrawEngineGUI(const Camera& camera, FileManager& fileManager, LevelManager& levelManager, ProjectManager& projectManager);
	void DrawRuntimeGUI();
	void DrawRuntimePreviewGUI();
	void DrawCreatorGUI(const Camera& camera);
	void EndFrame();
	void ApplyProjectState(
		bool editorShowAxis,
		bool editorShowGrid,
		const std::vector<std::string>& sceneAssets,
		const std::string& activeAssetName,
		const std::string& imguiLayout);

	void SetMode(FrontEndMode mode);
	FrontEndMode FrontEndModeValue() const;
	void OpenGameGUICreator();
	void ReturnToEngineGUIEditor();
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

