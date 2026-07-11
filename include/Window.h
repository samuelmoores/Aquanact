#pragma once
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

class Window {
public:
	Window();
	~Window();
	GLFWwindow* GLFW() { return m_glfwWindow; }

private:
	GLFWwindow* m_glfwWindow = nullptr;
};
