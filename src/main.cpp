#include <iostream>
#include <chrono>
#include <glad/glad.h>
#include <Engine.h>

void AquanactLoop()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	Engine::Tick();
	//std::cout << "fps: " << 1.0f/Engine::DeltaFrameTime() << std::endl;
	Engine::Input->Loop();
	Engine::UI->Loop();
	Engine::Renderer->Loop();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(Engine::Window->GLFW());
	glfwPollEvents();
}

void Shutdown()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
}

int main() 
{
	Engine::Init();
	glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	Engine::Input->SetObjects();
	Engine::Camera->SetObjects();

	while (Engine::Running())
	{
		AquanactLoop();
	}

	Shutdown();
	return 0;
}


