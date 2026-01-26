#pragma once
#include <Renderer.h>
#include <Window.h>
#include <UI.h>
#include <Level.h>
#include <Input.h>
#include "GLFW/glfw3.h"
#include <chrono>

class Engine {
public:
	//Delete Copy constructor
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	static bool Running();
	static float DeltaFrameTime();
	static float TimeElapsed();
	static void Tick();
	static void ToggleAxis();

	static Engine& Init() {
		static Engine instance;
		return instance;
	}

	static Window* Window;
	static Renderer* Renderer;
	static Camera* Camera;
	static UI* UI;
	static Level* Level;
	static Input* Input;
	static float m_deltaFrameTime;
	static std::chrono::steady_clock::time_point m_prevFrameTime;
	static float m_timeElapsed;
private:
	Engine();
};
