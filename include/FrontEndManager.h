#pragma once

#include <memory>

class Window;
class Camera;
class FileManager;
class SceneManager;
class ProjectManager;
#include "EngineGUI.h"

enum class FrontEndMode {
	Editor,
	Game
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
	FrontEndMode Mode() const;

	bool IsEditorMode() const;
	bool IsGameMode() const;
	EngineGUI& EditorGUI();
	const EngineGUI& EditorGUI() const;

private:
	FrontEndMode m_mode = FrontEndMode::Editor;
	std::unique_ptr<EngineGUI> m_engineGUI;
};
