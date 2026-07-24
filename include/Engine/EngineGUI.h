#pragma once

#include <string>

class Window;
class Camera;
class FileManager;
class LevelManager;
class ProjectManager;
class Entity;

class EngineGUI {
public:
	EngineGUI() = default;
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw(const Camera& camera, FileManager& fileManager, LevelManager& levelManager, ProjectManager& projectManager);
	void EndFrame();

	bool ShowAxis() const;
	bool ShowGrid() const;
	void SetShowAxis(bool showAxis);
	void SetShowGrid(bool showGrid);

private:
	void DrawBuildGamePopup();
	void DrawAddCodeFilePopup();
	void DrawNewLevelPopup();
	void DrawAddComponentPopup(Entity& entity);
	static std::string NormalizeGameClassName(const std::string& input);
	static std::string MakeHeaderTemplate(const std::string& className);
	static std::string MakeSourceTemplate(const std::string& className);
	static std::string NormalizeLevelName(const std::string& input);
	void CreateGameCodeFile(const std::string& className);

	Window* m_window = nullptr;
	bool m_showAxis = true;
	bool m_showGrid = true;
	bool m_initialized = false;
	unsigned int m_bootTexture = 0;
	int m_bootTextureWidth = 0;
	int m_bootTextureHeight = 0;
	int m_selectedLevelObjectIndex = -1;
	bool m_buildGamePopupRequested = false;
	bool m_addCodeFilePopupRequested = false;
	bool m_newLevelPopupRequested = false;
	bool m_addComponentPopupRequested = false;
	bool m_addCodeFileCreated = false;
	char m_newCodeFileName[128] = "PlayerHealth";
	char m_newLevelName[128] = "Level1";
	int m_selectedComponentEntityIndex = -1;
	std::string m_addCodeFileStatusMessage;
	std::string m_newLevelStatusMessage;
};
