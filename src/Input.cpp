#include "Input.h"
#include "Engine.h"

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

glm::vec2 Input::MoveInput() const
{
	float fwd = (IsDown(Action::MoveForward) ? 1.0f : 0.0f) - (IsDown(Action::MoveBack)  ? 1.0f : 0.0f);
	float rgt = (IsDown(Action::MoveRight)   ? 1.0f : 0.0f) - (IsDown(Action::MoveLeft)  ? 1.0f : 0.0f);
	return glm::vec2(fwd, rgt);
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
}
