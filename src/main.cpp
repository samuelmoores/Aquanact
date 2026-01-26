#include <iostream>
#include <chrono>
#include <glad/glad.h>
#include <Object3D.h>
#include <Animator.h>
#include <Engine.h>
#include <Windows.h>
#include <Input.h>

std::vector<Object3D*> objects;


void AquanactLoop()
{
	Engine::Input->Loop();
	Engine::UI->Loop();
	Engine::Renderer->Loop();
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
	objects = Engine::Level->Objects();
	Engine::Input->SetObjects();

	objects[1]->Move(glm::vec3(0.0f, 0.0f, -250.0f));

	std::cout << "Renderer: " << glGetString(GL_RENDERER);


	while (Engine::Running())
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		Engine::Tick();
		std::cout << "fps: " << 1.0f/Engine::DeltaFrameTime() << std::endl;
		AquanactLoop();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(Engine::Window->GLFW());
		glfwPollEvents();
	}
	Shutdown();
	return 0;
}


