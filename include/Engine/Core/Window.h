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

private:
	GLFWwindow* m_glfwWindow = nullptr;
};

