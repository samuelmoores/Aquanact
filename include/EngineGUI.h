#pragma once

class Window;
class Camera;

class EngineGUI {
public:
	EngineGUI() = default;
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw(const Camera& camera);
	void EndFrame();

	bool ShowAxis() const;
	bool ShowGrid() const;

private:
	bool m_showAxis = true;
	bool m_showGrid = true;
	bool m_initialized = false;
};
