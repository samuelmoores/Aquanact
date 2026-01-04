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

int main() 
{
	Engine::Init();

	//load OpenGL context
	gladLoadGL();

	//******************** setting up the scene *************************
	Axis axis(10.0f);
	std::vector<Object3D> sceneObjects;
	Object3D* selectedObject = nullptr;
	
	sceneObjects.push_back(Object3D("models/TestDummy.fbx", "models/TestDummy_Diffuse.png", true));
	sceneObjects.push_back(Object3D(Object3D::cubeVertices, Object3D::cubeFaces));
	sceneObjects[1].Move(glm::vec3(-2, 0, -3));

	int index = 0;
	sceneObjects[0].GetShader()->activate();
	sceneObjects[0].GetShader()->setUniform("bone", index);
	//******************** setting up the scene *************************

	//keyboard events
	glfwSetKeyCallback(Engine::Window, key_callback);

	//window event
	glfwSetFramebufferSizeCallback(Engine::Window, framebuffer_size_callback);

	//Start Loop
	glEnable(GL_DEPTH_TEST);

	while (!glfwWindowShouldClose(Engine::Window))
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		axis.UpdateProjection(Engine::Camera->GetProjectionMatrix());
		axis.draw(Engine::Camera->GetViewMatrix());

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

		/* Swap front and back buffers */
		glfwSwapBuffers(Engine::Window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}


