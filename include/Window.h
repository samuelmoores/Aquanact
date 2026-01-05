#pragma once
#include "GLFW/glfw3.h"

class Window {
public:
	Window();
	GLFWwindow* GLFW() { return m_glfwWindow; }

private:
	GLFWwindow* m_glfwWindow;
};
