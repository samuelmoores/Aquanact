#include <iostream>
#include <glad/glad.h>
#include <Object3D.h>
#include <Animator.h>
#include <Engine.h>
#include <Windows.h>
#include <chrono>

std::vector<Object3D*> objects;
glm::vec2 mouseLast(0);
glm::vec2 mouseCurr(0);
bool mouseDown = false;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	Engine::Camera->CameraControl(yoffset);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// Forward to ImGui first
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

	ImGuiIO& io = ImGui::GetIO();

	// Only process for your game if ImGui doesn't want it
	if (io.WantCaptureMouse) {
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		mouseDown = true;

		double xpos, ypos;
		glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
		mouseLast = glm::vec2(xpos, ypos);

	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		mouseDown = false;
	}

}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_W && action == GLFW_PRESS)
	{
		objects[0]->GetMesh()->SetAnim(1);

		//get degress and check should we rotate?
		float rotY = glm::degrees(objects[0]->Rotation().y);

		//if we are not facing forward, rotate forward
		if (rotY != 0.0f)
		{
			//negate current rotation
			float rotY_rad = objects[0]->Rotation().y;
			objects[0]->Rotate(glm::vec3(0, -rotY_rad, 0));
		}

	}

	
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
	{
		objects[0]->GetMesh()->SetAnim(1);

		//should we rotate?
		float rotY = glm::degrees(objects[0]->Rotation().y);

		//if we are not facing backwards, rotate backwards
		if (rotY != -180.0f)
		{
			//negate current rotation
			float rotY_rad = objects[0]->Rotation().y;
			objects[0]->Rotate(glm::vec3(0, -rotY_rad, 0));

			//rotate backwards
			objects[0]->Rotate(glm::vec3(0, glm::radians(-180.0f),  0));
			rotY = glm::degrees(objects[0]->Rotation().y);
		}
	}

	if (key == GLFW_KEY_D && action == GLFW_PRESS)
	{
		objects[0]->GetMesh()->SetAnim(1);

		//should we rotate?
		float rotY = glm::degrees(objects[0]->Rotation().y);

		//if we are not facing backwards, rotate backwards
		if (rotY != -90.0f)
		{
			//negate current rotation
			float rotY_rad = objects[0]->Rotation().y;
			objects[0]->Rotate(glm::vec3(0, -rotY_rad, 0));

			objects[0]->Rotate(glm::vec3(0, glm::radians(-90.0f), 0));
			rotY = glm::degrees(objects[0]->Rotation().y);
		}
	}

	if (key == GLFW_KEY_A && action == GLFW_PRESS)
	{
		objects[0]->GetMesh()->SetAnim(1);

		//should we rotate?
		float rotY = glm::degrees(objects[0]->Rotation().y);

		//if we are not facing backwards, rotate backwards
		if (rotY != 90.0f)
		{
			//negate current rotation
			float rotY_rad = objects[0]->Rotation().y;
			objects[0]->Rotate(glm::vec3(0, -rotY_rad, 0));

			objects[0]->Rotate(glm::vec3(0, glm::radians(90.0f), 0));
			rotY = glm::degrees(objects[0]->Rotation().y);
		}
	}

	if ((key == GLFW_KEY_W || key == GLFW_KEY_S || key == GLFW_KEY_D || key == GLFW_KEY_A) && action == GLFW_RELEASE)
		objects[0]->GetMesh()->SetAnim(0);

}


void AquanactLoop()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	Engine::Tick();

	if (mouseDown)
	{
		double xpos, ypos;
		glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
		mouseCurr = glm::vec2(xpos, ypos);
		glm::vec2 mouseDiff = mouseCurr - mouseLast;
		Engine::Camera->CameraControl(mouseDiff);
		mouseLast = mouseCurr;
	}

	float moveSpeed = 40*Engine::DeltaFrameTime()*100000.0f;

	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_W) == GLFW_PRESS)
		objects[0]->Move(glm::vec3(0.0f, 0.0f, moveSpeed));

	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_S) == GLFW_PRESS)
		objects[0]->Move(glm::vec3(0.0f, 0.0f, -moveSpeed));

	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_D) == GLFW_PRESS)
		objects[0]->Move(glm::vec3(-moveSpeed, 0.0f, 0.0f));

	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_A) == GLFW_PRESS)
		objects[0]->Move(glm::vec3(moveSpeed, 0.0f, 0.0f));

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

	glfwSetScrollCallback(Engine::Window->GLFW(), scroll_callback);
	glfwSetMouseButtonCallback(Engine::Window->GLFW(), mouse_button_callback);
	glfwSetKeyCallback(Engine::Window->GLFW(), key_callback);

	objects = Engine::Level->Objects();

	while (Engine::Running())
	{
		AquanactLoop();
	}
	Shutdown();
	return 0;
}


