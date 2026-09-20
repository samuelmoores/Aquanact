#pragma once
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

class Window {
public:
	Window() = default;
	~Window();
	void startUp();
	void shutDown();
	GLFWwindow* GLFW() { return m_glfwWindow; }
	void GetFramebufferSize(int& width, int& height) const;
	bool ShouldClose() const;
	void SwapBuffers();
	void PollEvents();
	void Focus();
	void ToggleFullscreen();
	bool IsFullscreen() const { return m_fullscreen; }
	double RefreshRate() const;

private:
	GLFWwindow* m_glfwWindow = nullptr;
	bool m_fullscreen = false;
	int m_windowedX = 100;
	int m_windowedY = 100;
	int m_windowedWidth = 1280;
	int m_windowedHeight = 720;
};

