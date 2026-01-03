#pragma once
#include "GLFW/glfw3.h"

class Window {
public:
	//Delete Copy constructor
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

	static Window& Init() {
		static Window instance;
		return instance;
	}

	float getWidth();
	float getHeight();
    static GLFWwindow* Engine;
private:
	Window();
};
