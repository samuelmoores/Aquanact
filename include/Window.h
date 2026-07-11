#pragma once
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

class Window {
public:
	Window() = default;
	~Window();
	void startUp();
	GLFWwindow* GLFW() { return m_glfwWindow; }

private:
	GLFWwindow* m_glfwWindow = nullptr;
};
