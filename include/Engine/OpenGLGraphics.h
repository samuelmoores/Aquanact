#pragma once

class Window;

class OpenGLGraphics {
public:
	OpenGLGraphics() = default;
	~OpenGLGraphics();

	void startUp(Window& window);
	void shutDown();

	void MakeCurrent();
	void SwapBuffers();
	void SetVSync(bool enabled);
	void UpdateViewport();
	void ConfigureDefaultState();
	// GUI rendering needs a different state than the 3D scene pass. This helper
	// exists so overlays like MyGUI can draw without inheriting depth or shader state.
	void ConfigureGuiState();

	bool IsInitialized() const { return m_initialized; }

private:
	Window* m_window = nullptr;
	bool m_initialized = false;
	bool m_vsyncEnabled = true;
	int m_lastViewportWidth = 0;
	int m_lastViewportHeight = 0;
};
