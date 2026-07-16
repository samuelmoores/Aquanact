#pragma once

#include <memory>
#include "Globals.h"

class Window;
class Camera;
class FileManager;
class SceneManager;
class ProjectManager;
#include "EngineGUI.h"

class FrontEndManager {
public:
	FrontEndManager() = default;
	~FrontEndManager();

	void startUp(Window& window);
	void shutDown();

	void BeginFrame();
	void Draw(const Camera& camera, FileManager& fileManager, SceneManager& sceneManager, ProjectManager& projectManager);
	void EndFrame();

	EngineMode Mode() const;

	bool IsEditorMode() const;
	bool IsGameMode() const;
	EngineGUI& EditorGUI();
	const EngineGUI& EditorGUI() const;

private:
	std::unique_ptr<EngineGUI> m_engineGUI;
};
