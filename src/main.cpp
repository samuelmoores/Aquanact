#include <iostream>
#include <chrono>
#include <glad/glad.h>
#include <Engine.h>
#include "PlayerController.h"

int main()
{
	Engine::Init();
	PlayerController playerController(Engine::Level->Objects());
	Engine::Camera->SetObjects();

	while (Engine::Running())
	{
		Engine::Tick();
		Engine::Input->Loop();
		playerController.Update();
		Engine::Renderer->Loop();
	}

	glfwTerminate();
	return 0;
}
