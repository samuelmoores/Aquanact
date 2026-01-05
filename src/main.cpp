#include <iostream>
#include <glad/glad.h>
#include <Object3D.h>
#include <Animator.h>
#include <Engine.h>

void AquanactLoop()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
	while (Engine::Running())
	{
		AquanactLoop();
	}
	Shutdown();
	return 0;
}


