#include <iostream>
#include <chrono>
#include <glad/glad.h>
#include <Engine.h>

void AquanactLoop()
{
	Engine::Tick();
	//std::cout << "fps: " << 1.0f/Engine::DeltaFrameTime() << std::endl;

	Engine::Input->Loop();
	//Gameplay logic here
	Engine::Renderer->Loop();
}

void Shutdown()
{
	glfwTerminate();
}

int main() 
{
	Engine::Init();
	Engine::Input->SetObjects();
	Engine::Camera->SetObjects();

	while (Engine::Running())
	{
		AquanactLoop();
	}

	Shutdown();
	return 0;
}


