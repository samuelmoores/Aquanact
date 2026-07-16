#pragma once

class Window;
class Camera;

class UICreator {
public:
	UICreator() = default;
	void startUp(Window& window);
	void shutDown();
	void BeginFrame();
	void Draw(const Camera& camera);
	void EndFrame();

private:
	Window* m_window = nullptr;
	bool m_initialized = false;
	bool m_showCreatePopup = false;
};
