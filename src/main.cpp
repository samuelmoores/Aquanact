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
glm::vec3 moveDirection = glm::vec3(0);
bool blendRot = false;
float currRot = 0.0f;
float nextRot = 0.0f;
float startRot = 0.0f;

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

	glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

}

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

	if (wPressed)
	{
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(1);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}

		moveDirection.z += 1.0f;
	}
	if (sPressed)
	{
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(1);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
		moveDirection.z -= 1.0f;
	}
	if (aPressed)
	{
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(1);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
		moveDirection.x += 1.0f;
	}
	if (dPressed)
	{
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(1);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
		moveDirection.x -= 1.0f;
	}
	if (wReleased)
	{
		moveDirection.z -= 1.0f;
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
	}
	if (sReleased)
	{
		moveDirection.z += 1.0f;
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
	}
	if (aReleased)
	{
		moveDirection.x -= 1.0f;
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
	}
	if (dReleased)
	{
		moveDirection.x += 1.0f;
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
	}

	// Update animation and rotation
	if (glm::length(moveDirection) > 0.0f)
	{
		startRot = currRot;
		nextRot = atan2(moveDirection.x, moveDirection.z);
		blendRot = true;
	}

}

void AquanactLoop()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	Engine::Tick();

	//std::cout << "fps: " << 1.0f/Engine::DeltaFrameTime() << std::endl;

	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	double xpos, ypos;
	glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
	mouseCurr = glm::vec2(xpos, ypos);
	glm::vec2 mouseDiff = mouseCurr - mouseLast;
	Engine::Camera->CameraControl(mouseDiff);
	mouseLast = mouseCurr;
	
	float moveSpeed = 100.0f;

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


	if (blendRot)
	{
		if ((glm::degrees(startRot) <= -90 && glm::degrees(startRot) >= -135 ) && glm::degrees(nextRot) == 180)
		{
			currRot -= Engine::DeltaFrameTime() * 15.0f;

			if (glm::degrees(currRot) <= -180)
			{
				currRot = nextRot;
				blendRot = false;
			}

		}
		else if (glm::degrees(currRot) == 180 && (glm::degrees(nextRot) == -90 || glm::degrees(nextRot) == -135))
		{
			currRot = glm::radians(-180.0f);
			currRot -= Engine::DeltaFrameTime() * 15.0f;
		}
		else if (currRot > nextRot)
		{
			currRot -= Engine::DeltaFrameTime() * 15.0f;
			if (currRot <= nextRot)
			{	
				currRot = nextRot;
				blendRot = false;
			}
		}
		else if (currRot < nextRot)
		{
			currRot += Engine::DeltaFrameTime()* 15.0f;
			if (currRot >= nextRot)
			{
				currRot = nextRot;
				blendRot = false;
			}
		}

		objects[0]->SetRotation(glm::vec3(0.0f, currRot, 0.0f));
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


