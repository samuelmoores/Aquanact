#pragma once
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

class Window {
public:
	Window() = default;
	~Window();
	void startUp();
	void shutDown();
	GLFWwindow* GLFW() { return m_glfwWindow; }
	bool ShouldClose() const;
	void SwapBuffers();
	void PollEvents();

private:
	GLFWwindow* m_glfwWindow = nullptr;
};
