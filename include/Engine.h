#pragma once
#include <Renderer.h>
#include <Window.h>
#include <UI.h>
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

	static Window* Window;
	static Renderer* Renderer;
	static Camera* Camera;
	static UI* UI;
private:
	Engine();
};
