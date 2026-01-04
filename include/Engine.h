#pragma once
#include <Renderer.h>
#include "GLFW/glfw3.h"

class Engine {
public:
	//Delete Copy constructor
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	static Engine& Init() {
		static Engine instance;
		return instance;
	}

	static GLFWwindow* Window;
	static Renderer* Renderer;
	static Camera* Camera;
private:
	Engine();
};
