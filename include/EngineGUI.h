#pragma once

class Window;
class Camera;
class FileManager;

class EngineGUI {
public:
	EngineGUI() = default;
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw(const Camera& camera, FileManager& fileManager);
	void EndFrame();

	bool ShowAxis() const;
	bool ShowGrid() const;

private:
	Window* m_window = nullptr;
	bool m_showAxis = true;
	bool m_showGrid = true;
	bool m_initialized = false;
};
