#include <iostream>
#include <chrono>
#include <glad/glad.h>
#include <Object3D.h>
#include <Animator.h>
#include <Engine.h>
#include <Windows.h>
#include <Input.h>

std::vector<Object3D*> objects;
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


//zoom camera in and out
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	Engine::Camera->CameraControl(yoffset);
}

//WASD press and release
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

	// Update player rotation to direction of movement
	if (glm::length(moveDirection) > 0.0f)
	{
		startRot = currRot;
		nextRot = atan2(moveDirection.x, moveDirection.z);
		blendRot = true;
	}
	else
	{
		move = false;
	}

}

//Collision detect
bool AABBIntersect(const glm::vec3& minA, const glm::vec3& maxA, const glm::vec3& minB, const glm::vec3& maxB)
{
	bool overlapX = (minA.x <= maxB.x && maxA.x >= minB.x);
	bool overlapY = (minA.y <= maxB.y && maxA.y >= minB.y);
	bool overlapZ = (minA.z <= maxB.z && maxA.z >= minB.z);

	//std::cout << "AABB Check:\n";
	//std::cout << "  A min: " << minA.x << ", " << minA.y << ", " << minA.z << "\n";
	//std::cout << "  A max: " << maxA.x << ", " << maxA.y << ", " << maxA.z << "\n";
	//std::cout << "  B min: " << minB.x << ", " << minB.y << ", " << minB.z << "\n";
	//std::cout << "  B max: " << maxB.x << ", " << maxB.y << ", " << maxB.z << "\n";

	//std::cout << "  Overlap X: " << overlapX << "\n";
	//std::cout << "  Overlap Y: " << overlapY << "\n";
	//std::cout << "  Overlap Z: " << overlapZ << "\n";

	//if any are not overlapping, there is no collision
	bool result = overlapX && overlapY && overlapZ;
	//std::cout << "  COLLISION: " << result << "\n\n";

	return result;
}

float ShortestAngleDelta(float from, float to)
{
	// angualar difference
	float delta = to - from;

	// if angualar distance is greater than pi, minus 2pi
	// just like the unit circle in calculus
	while (delta > glm::pi<float>()) delta -= glm::two_pi<float>();
	while (delta < -glm::pi<float>()) delta += glm::two_pi<float>();

	return delta;
}

void AquanactLoop()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	Engine::Tick();
	Engine::Input->Loop();
	float moveSpeed = 75.0f;
	glm::vec3 movement(0.0f);
	//std::cout << "fps: " << 1.0f/Engine::DeltaFrameTime() << std::endl;



	//************* These are all called AFTER the key callback ************
	//************* For keys being held down, key callback does not register it fast enough ************
	// continuously set move to true if WASD is held down
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

	//================ Collision detection =================================
	// player is first scene object by default
	// wall is second
	// get min and max
	Mesh* playerMesh = objects[0]->GetMesh();
	Mesh* wallMesh = objects[1]->GetMesh();
	glm::vec3 playerMin = playerMesh->minBounds();
	glm::vec3 playerMax = playerMesh->maxBounds();
	glm::vec3 wallMin = wallMesh->minBounds();
	glm::vec3 wallMax = wallMesh->maxBounds();

	// move is set by keyboard input
	if (move)
	{
		movement = glm::normalize(moveDirection) * moveSpeed * Engine::DeltaFrameTime();
	}

	// Predict next AABB
	glm::vec3 nextMin = playerMin + movement;
	glm::vec3 nextMax = playerMax + movement;

	bool collided = AABBIntersect(nextMin, nextMax, wallMin, wallMax);

	// Apply movement only if no collision
	if (!collided)
	{
		objects[0]->Move(movement);
	}


	//============================= Rotation ===========================================
	if (blendRot)
	{
		const float speed = 15.0f;
		float dt = Engine::DeltaFrameTime();

		// dont rotate more than 180 degrees
		// this is the start of the angle to rotate by
		float delta = ShortestAngleDelta(currRot, nextRot);

		// create exponential smoothing  (e^x)
		// higher speed -> snappy rotation
		// lower speed  -> floaty rotation
		float t = 1.0f - exp(-speed * dt);

		// smooth out delta before adding to current rotation
		currRot += delta * t;

		// floating absolute value
		// once you are .5 radians away, stop rotating
		if (fabs(delta) < glm::radians(0.5f))
		{
			currRot = nextRot;
			blendRot = false;
		}

		//player is first scene object by default
		objects[0]->SetRotation({ 0.0f, currRot, 0.0f });
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
	glfwSetKeyCallback(Engine::Window->GLFW(), key_callback);
	glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	objects = Engine::Level->Objects();

	objects[1]->Move(glm::vec3(0.0f, 0.0f, -250.0f));



	while (Engine::Running())
	{
		AquanactLoop();
	}
	Shutdown();
	return 0;
}


