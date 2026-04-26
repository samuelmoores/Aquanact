#include "Input.h"
#include "Engine.h"

static bool AABBIntersect(const glm::vec3& minA, const glm::vec3& maxA, const glm::vec3& minB, const glm::vec3& maxB)
{
	return (minA.x <= maxB.x && maxA.x >= minB.x)
		&& (minA.y <= maxB.y && maxA.y >= minB.y)
		&& (minA.z <= maxB.z && maxA.z >= minB.z);
}

static float ShortestAngleDelta(float from, float to)
{
	float delta = to - from;
	while (delta >  glm::pi<float>()) delta -= glm::two_pi<float>();
	while (delta < -glm::pi<float>()) delta += glm::two_pi<float>();
	return delta;
}

void Input::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	Input* self = static_cast<Input*>(glfwGetWindowUserPointer(window));
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	self->m_windowActive = true;
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	self->m_mouseLast = glm::vec2(xpos, ypos);
}

void Input::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	Engine::Camera->CameraControl(yoffset);
}

Input::Input()
{
	GLFWwindow* win = Engine::Window->GLFW();
	glfwSetWindowUserPointer(win, this);
	glfwSetMouseButtonCallback(win, MouseButtonCallback);
	glfwSetScrollCallback(win, ScrollCallback);

	double xpos, ypos;
	glfwGetCursorPos(win, &xpos, &ypos);
	m_mouseLast = glm::vec2(xpos, ypos);

	m_bindings[GLFW_KEY_W]      = Action::MoveForward;
	m_bindings[GLFW_KEY_S]      = Action::MoveBack;
	m_bindings[GLFW_KEY_A]      = Action::MoveLeft;
	m_bindings[GLFW_KEY_D]      = Action::MoveRight;
	m_bindings[GLFW_KEY_ESCAPE] = Action::Escape;
}

void Input::UpdateActionStates()
{
	GLFWwindow* win = Engine::Window->GLFW();
	for (auto& [key, action] : m_bindings)
	{
		bool wasDown = m_actions[action].isDown;
		bool nowDown = glfwGetKey(win, key) == GLFW_PRESS;
		m_actions[action].isDown       = nowDown;
		m_actions[action].justPressed  = !wasDown && nowDown;
		m_actions[action].justReleased = wasDown && !nowDown;
	}
}

bool Input::IsDown(Action a) const
{
	auto it = m_actions.find(a);
	return it != m_actions.end() && it->second.isDown;
}

bool Input::JustPressed(Action a) const
{
	auto it = m_actions.find(a);
	return it != m_actions.end() && it->second.justPressed;
}

bool Input::JustReleased(Action a) const
{
	auto it = m_actions.find(a);
	return it != m_actions.end() && it->second.justReleased;
}

void Input::SetObjects()
{
	m_objects = Engine::Level->Objects();
}

void Input::Loop()
{
	UpdateActionStates();

	if (IsDown(Action::Escape))
	{
		glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		m_windowActive = false;
	}

	if (!m_windowActive)
		return;

	// Mouse look
	double xpos, ypos;
	glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
	m_mouseCurr = glm::vec2(xpos, ypos);
	Engine::Camera->CameraControl(m_mouseCurr - m_mouseLast);
	m_mouseLast = m_mouseCurr;

	// Movement direction
	glm::vec3 forward = Engine::Camera->Forward();
	glm::vec3 right   = Engine::Camera->Right();
	m_moveDirection   = glm::vec3(0);
	m_move            = false;

	if (IsDown(Action::MoveForward)) { m_moveDirection += forward; m_move = true; }
	if (IsDown(Action::MoveBack))    { m_moveDirection -= forward; m_move = true; }
	if (IsDown(Action::MoveRight))   { m_moveDirection += right;   m_move = true; }
	if (IsDown(Action::MoveLeft))    { m_moveDirection -= right;   m_move = true; }

	// Animation: trigger on move start/stop transitions
	bool isMoving = m_move;
	if (!m_wasMoving && isMoving)
	{
		m_objects[0]->GetMesh()->SetNextAnim(1);
		m_objects[0]->StartAnimBlend();
	}
	if (m_wasMoving && !isMoving)
	{
		m_objects[0]->GetMesh()->SetNextAnim(0);
		m_objects[0]->StartAnimBlend();
	}
	m_wasMoving = isMoving;

	// Collision + movement
	const float moveSpeed = 250.0f;
	glm::vec3 movement(0.0f);
	if (m_move && glm::length(m_moveDirection) > 0.1f)
		movement = glm::normalize(m_moveDirection) * moveSpeed * Engine::DeltaFrameTime();

	Mesh* playerMesh = m_objects[0]->GetMesh();
	glm::vec3 nextMin = playerMesh->minBounds() + movement;
	glm::vec3 nextMax = playerMesh->maxBounds() + movement;

	bool collided = false;
	for (int i = 1; i < (int)m_objects.size() - 1; i++)
	{
		Mesh* wallMesh = m_objects[i]->GetMesh();
		if (AABBIntersect(nextMin, nextMax, wallMesh->minBounds(), wallMesh->maxBounds()))
		{
			collided = true;
			break;
		}
	}

	if (!collided)
		m_objects[0]->Move(movement);

	// Rotation smoothing
	if (glm::length(m_moveDirection) > 0.01f)
	{
		m_nextRot  = atan2(m_moveDirection.x, m_moveDirection.z);
		m_blendRot = true;
	}

	if (m_blendRot)
	{
		const float speed = 15.0f;
		float delta = ShortestAngleDelta(m_currRot, m_nextRot);
		float t     = 1.0f - exp(-speed * Engine::DeltaFrameTime());
		m_currRot  += delta * t;
		if (fabs(delta) < glm::radians(0.5f))
		{
			m_currRot  = m_nextRot;
			m_blendRot = false;
		}
		m_objects[0]->SetRotation({ 0.0f, m_currRot, 0.0f });
	}
}
