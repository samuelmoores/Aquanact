#include "Input.h"
#include "Engine.h"

bool windowActive = true;
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

std::vector<Object3D*> objects_input;


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
	}
	if (sPressed)
	{
		//make sure both s and w are not pressed, canceling each other out
		//if so, you do not want to play move anim
		
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(1);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
	}
	if (aPressed)
	{
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(1);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
	}
	if (dPressed)
	{
		//play idle anim
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(1);
			Engine::Level->Objects()[0]->StartAnimBlend();
		}
	}

	//============================ RELEASE ===========================================
	if (wReleased)
	{
		moveDirection -= Engine::Camera->Forward();
		std::cout << "move direction: " << moveDirection.x << ", " << moveDirection.z << std::endl;
		if (glm::length(moveDirection) < 0.1f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
			Engine::Level->Objects()[0]->StartAnimBlend();
			moveDirection = glm::vec3(0);
		}
		
	}
	if (sReleased)
	{
		moveDirection += Engine::Camera->Forward();
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
			Engine::Level->Objects()[0]->StartAnimBlend();
			moveDirection = glm::vec3(0);
		}
	}
	if (aReleased)
	{
		moveDirection += Engine::Camera->Right();
		if (glm::length(moveDirection) == 0.0f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
			Engine::Level->Objects()[0]->StartAnimBlend();
			moveDirection = glm::vec3(0);
		}
	}
	if (dReleased)
	{
		moveDirection -= Engine::Camera->Right();
		if (glm::length(moveDirection) < 0.1f)
		{
			Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
			Engine::Level->Objects()[0]->StartAnimBlend();
			moveDirection = glm::vec3(0);
		}
		
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

Input::Input()
{
	glfwSetMouseButtonCallback(Engine::Window->GLFW(), mouse_button_callback);
	glfwSetScrollCallback(Engine::Window->GLFW(), scroll_callback);
	glfwSetKeyCallback(Engine::Window->GLFW(), key_callback);

	double xpos, ypos;
	glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
	mouseLast = glm::vec2(xpos, ypos);


}

bool almostEqual(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f) {
	return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3(epsilon)));
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

	float moveSpeed = 75.0f;
	glm::vec3 movement(0.0f);
	glm::vec3 forward = Engine::Camera->Forward();
	glm::vec3 right = Engine::Camera->Right();
	moveDirection = glm::vec3(0);

	//************* These are all called AFTER the key callback ************
	//************* For keys being held down, key callback does not register it fast enough ************
	// continuously set move to true if WASD is held down
	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_W) == GLFW_PRESS)
	{
		moveDirection += forward;
		move = wPressed = true;
	}
	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_S) == GLFW_PRESS)
	{
		moveDirection -= forward;
		move = sPressed = true;
	}
	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_D) == GLFW_PRESS)
	{
		moveDirection += right;
		move = true;
	}
	if (glfwGetKey(Engine::Window->GLFW(), GLFW_KEY_A) == GLFW_PRESS)
	{
		moveDirection -= right;
		move = true;
	}

	//two opposite keys are pressed, play idle


	//================ Collision detection =================================
	// player is first scene object by default
	// wall is second
	// get min and max
	Mesh* playerMesh = objects_input[0]->GetMesh();
	glm::vec3 playerMin = playerMesh->minBounds();
	glm::vec3 playerMax = playerMesh->maxBounds();

	// move is set by keyboard input
	if (move && glm::length(moveDirection) > 0.1f)
	{
		movement = glm::normalize(moveDirection) * moveSpeed * Engine::DeltaFrameTime();
	}
	else
	{
		std::cout << "dont call move\n";
		//Engine::Level->Objects()[0]->GetMesh()->SetNextAnim(0);
		//Engine::Level->Objects()[0]->StartAnimBlend();
	}

	// Predict next AABB
	glm::vec3 nextMin = playerMin + movement;
	glm::vec3 nextMax = playerMax + movement;

	bool collided = false;
	//first object player, last object floor
	for (int i = 1; i < objects_input.size() - 1; i++)
	{
		Mesh* wallMesh = objects_input[i]->GetMesh();
		glm::vec3 wallMin = wallMesh->minBounds();
		glm::vec3 wallMax = wallMesh->maxBounds();
		collided = AABBIntersect(nextMin, nextMax, wallMin, wallMax);

		if (collided)
			break;
	}


	// Apply movement only if no collision
	if (!collided)
	{
		objects_input[0]->Move(movement);
	}


	//============================= Rotation ===========================================
	//if player is moving, update rotation
	if (glm::length(moveDirection) > 0.01f)
	{
		startRot = currRot;
		nextRot = atan2(moveDirection.x, moveDirection.z);
		blendRot = true;
	}
	else
	{
		move = false;
	}

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
		objects_input[0]->SetRotation({ 0.0f, currRot, 0.0f });
	}
}

void Input::SetObjects()
{
	objects_input = Engine::Level->Objects();
}
