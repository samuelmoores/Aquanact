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

	/* Swap front and back buffers */
	glfwSwapBuffers(Engine::Window->GLFW());

	/* Poll for and process events */
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


