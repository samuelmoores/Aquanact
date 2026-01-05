#include <iostream>
#include <glad/glad.h>
#include <Axis.h>
#include <Object3D.h>
#include <Animator.h>
#include <Engine.h>

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_E && action == GLFW_PRESS)
		std::cout << "[E]\n";
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) 
{
	glfwGetWindowSize(window, &width, &height);
	glViewport(0, 0, width, height);
}

void Import()
{
	std::cout << "Import\n";
}

void AquanactLoop(Axis axis, std::vector<Object3D> sceneObjects)
{
	axis.UpdateProjection(Engine::Camera->GetProjectionMatrix());
	axis.draw(Engine::Camera->GetViewMatrix());

	//========================== ImGUI ===================================

	// Start frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Your UI code here
	ImGui::Begin("Window");
	
	if (ImGui::Button("Click"))
	{
		Import();
	}

	ImGui::End();

	// Render
	ImGui::Render();

	//========================== ImGUI ===================================


	for (int i = 0; i < sceneObjects.size(); i++)
	{
		RenderCommand rc;
		rc.mesh = sceneObjects[i].GetMesh();
		rc.shader = sceneObjects[i].GetShader();
		rc.modelMatrix = sceneObjects[i].BuildModelMatrix();
		rc.isSkinned = sceneObjects[i].skinned();
		Engine::Renderer->Submit(rc);
	}
	Engine::Renderer->Flush(Engine::Camera);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main() 
{
	Engine::Init();
	gladLoadGL();
	glfwSetKeyCallback(Engine::Window, key_callback);
	glfwSetFramebufferSizeCallback(Engine::Window, framebuffer_size_callback);
	glEnable(GL_DEPTH_TEST);

	Axis axis(10.0f);
	std::vector<Object3D> sceneObjects;
	sceneObjects.push_back(Object3D(Object3D::cubeVertices, Object3D::cubeFaces));

	while (!glfwWindowShouldClose(Engine::Window))
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		AquanactLoop(axis, sceneObjects);

		/* Swap front and back buffers */
		glfwSwapBuffers(Engine::Window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();

	return 0;
}


