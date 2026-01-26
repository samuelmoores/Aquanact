#include "Input.h"
#include "Engine.h"

bool windowActive = true;
glm::vec2 mouseLast(0);
glm::vec2 mouseCurr(0);

//Mouse press, ignore IMGUI or set window active
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// Forward to ImGui first
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

	ImGuiIO& io = ImGui::GetIO();

	// Only process for your game if ImGui doesn't want it
	if (io.WantCaptureMouse) {
		return;
	}

	glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	windowActive = true;
	double xpos, ypos;
	glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
	mouseLast = glm::vec2(xpos, ypos);
}

Input::Input()
{
	glfwSetMouseButtonCallback(Engine::Window->GLFW(), mouse_button_callback);

	double xpos, ypos;
	glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
	mouseLast = glm::vec2(xpos, ypos);
}

void Input::Loop()
{
	//press escape to get cursor back and make window inactive
	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		windowActive = false;
	}

	if (windowActive)
	{
		//calculate mouse movement to send to camera controller
		double xpos, ypos;
		glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
		mouseCurr = glm::vec2(xpos, ypos);
		glm::vec2 mouseDiff = mouseCurr - mouseLast;
		Engine::Camera->CameraControl(mouseDiff, Engine::Level->Objects()[1]->GetMesh());
		mouseLast = mouseCurr;
	}
}
