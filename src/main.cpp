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
static bool wPressed = false;
static bool sPressed = false;
static bool aPressed = false;
static bool dPressed = false;

static bool wReleased = false;
static bool sReleased = false;
static bool aReleased = false;
static bool dReleased = false;

bool move = false;


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

glm::vec3 moveDirection = glm::vec3(0);

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Update key states
	wPressed = (action == GLFW_PRESS && key == GLFW_KEY_W);
	sPressed = (action == GLFW_PRESS && key == GLFW_KEY_S);
	aPressed = (action == GLFW_PRESS && key == GLFW_KEY_A);
	dPressed = (action == GLFW_PRESS && key == GLFW_KEY_D);

	wReleased = (action == GLFW_RELEASE && key == GLFW_KEY_W);
	sReleased = (action == GLFW_RELEASE && key == GLFW_KEY_S);
	aReleased = (action == GLFW_RELEASE && key == GLFW_KEY_A);
	dReleased = (action == GLFW_RELEASE && key == GLFW_KEY_D);

	if (wPressed) moveDirection.z += 1.0f;
	if (sPressed) moveDirection.z -= 1.0f;
	if (aPressed) moveDirection.x += 1.0f;
	if (dPressed) moveDirection.x -= 1.0f;

	if (wReleased) moveDirection.z -= 1.0f;
	if (sReleased) moveDirection.z += 1.0f;
	if (aReleased) moveDirection.x -= 1.0f;
	if (dReleased) moveDirection.x += 1.0f;

	// Update animation and rotation
	if (glm::length(moveDirection) > 0.0f)
	{
		objects[0]->GetMesh()->SetAnim(1);
		float angle = atan2(moveDirection.x, moveDirection.z);
		objects[0]->SetRotation(glm::vec3(0.0f, angle, 0.0f));
	}
	else
	{
		objects[0]->GetMesh()->SetAnim(0);
	}
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
		Engine::Camera->CameraControl(mouseDiff, Engine::Level->Objects()[0]->Position());
		mouseLast = mouseCurr;
	}

	float moveSpeed = 20*100000.0f;

	//************* These are all called AFTER the key callback ************
	//************* For keys being held down, key callback does not register it fast enough ************
	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_W) == GLFW_PRESS)
	{
		move = wPressed = true;
	}

	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_S) == GLFW_PRESS)
	{
		move = sPressed = true;
	}

	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_D) == GLFW_PRESS)
	{
		move = true;
	}

	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_A) == GLFW_PRESS)
	{
		move = true;
	}

	if (move && glm::length(moveDirection) != 0)
	{ 
		objects[0]->Move(glm::normalize(glm::vec3(moveDirection)) * moveSpeed * Engine::DeltaFrameTime());
	}

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


