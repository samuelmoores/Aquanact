#include "Engine/Core/Input.h"

#include "Engine/Core/Window.h"
#include "Engine/Core/Root.h"
#include "GLFW/glfw3.h"

#include <cmath>
#include <algorithm>

#include <imgui.h>
#include <MYGUI/MyGUI_InputManager.h>
#include <MYGUI/MyGUI_MouseButton.h>

void Input::startUp(Window& window)
{
	m_window = &window;
	m_lookActive = false;
	m_lookBecameActive = false;
	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(m_window->GLFW(), &x, &y);
	m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
	m_mouseDelta = glm::vec2(0.0f);

	glfwSetWindowUserPointer(m_window->GLFW(), this);
	// Chain GLFW callbacks so ImGui keeps receiving input while MyGUI gets the
	// same raw mouse events in game mode.
	m_previousMouseButtonCallback = glfwSetMouseButtonCallback(m_window->GLFW(), &Input::MouseButtonCallback);
	m_previousCursorPosCallback = glfwSetCursorPosCallback(m_window->GLFW(), &Input::CursorPosCallback);
}

void Input::shutDown()
{
	if (m_window && m_lookActive) {
		glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	if (m_window)
	{
		glfwSetMouseButtonCallback(m_window->GLFW(), m_previousMouseButtonCallback);
		glfwSetCursorPosCallback(m_window->GLFW(), m_previousCursorPosCallback);
	}
	m_window = nullptr;
	m_previousMouseButtonCallback = nullptr;
	m_previousCursorPosCallback = nullptr;
	m_lookActive = false;
	m_ignoreMouseDeltaOnce = false;
	m_lookBecameActive = false;
	m_mouseDelta = glm::vec2(0.0f);
}

void Input::Update()
{
	if (!m_window) {
		return;
	}

	static double lastTime = glfwGetTime();
	double now = glfwGetTime();
	m_deltaTime = static_cast<float>(now - lastTime);
	lastTime = now;
	m_windowFocused = glfwGetWindowAttrib(m_window->GLFW(), GLFW_FOCUSED) == GLFW_TRUE;

	if (!m_windowFocused) {
		m_moveInput = glm::vec3(0.0f);
		m_lookBecameActive = false;
		m_mouseDelta = glm::vec2(0.0f);
		return;
	}

	m_moveInput = glm::vec3(0.0f);
	m_lookBecameActive = false;

	const bool gameMode = Root::Current().State().IsGameMode();
	const bool escapePressed = glfwGetKey(m_window->GLFW(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
	if (gameMode && (!m_windowFocused || escapePressed))
	{
		if (m_lookActive)
		{
			m_lookActive = false;
			m_ignoreMouseDeltaOnce = false;
			glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}
	else if (gameMode && !m_lookActive)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse || ImGui::IsAnyItemActive())
		{
			m_mouseDelta = glm::vec2(0.0f);
			return;
		}

		const bool mouseClick =
			glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ||
			glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS ||
			glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
		if (mouseClick)
		{
			m_lookActive = true;
			m_lookBecameActive = true;
			m_ignoreMouseDeltaOnce = true;
			double x = 0.0;
			double y = 0.0;
			glfwGetCursorPos(m_window->GLFW(), &x, &y);
			m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
			glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
	}
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_W) == GLFW_PRESS) m_moveInput.z += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_S) == GLFW_PRESS) m_moveInput.z -= 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_D) == GLFW_PRESS) m_moveInput.x += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_A) == GLFW_PRESS) m_moveInput.x -= 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_E) == GLFW_PRESS) m_moveInput.y += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_Q) == GLFW_PRESS) m_moveInput.y -= 1.0f;

	if (!gameMode)
	{
		const bool rightDown = glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
		if (rightDown && !m_lookActive) {
			m_lookActive = true;
			m_lookBecameActive = true;
			m_ignoreMouseDeltaOnce = true;
			double x = 0.0;
			double y = 0.0;
			glfwGetCursorPos(m_window->GLFW(), &x, &y);
			m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
			glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		} else if (!rightDown && m_lookActive) {
			m_lookActive = false;
			m_ignoreMouseDeltaOnce = false;
			glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	m_mouseDelta = glm::vec2(0.0f);
	if (m_lookActive) {
		if (m_ignoreMouseDeltaOnce)
		{
			double x = 0.0;
			double y = 0.0;
			glfwGetCursorPos(m_window->GLFW(), &x, &y);
			m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
			m_ignoreMouseDeltaOnce = false;
			return;
		}

		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(m_window->GLFW(), &x, &y);
		const glm::vec2 cursorPos(static_cast<float>(x), static_cast<float>(y));
		m_mouseDelta = cursorPos - m_lastCursorPos;
		const float mouseDeltaLength = glm::length(m_mouseDelta);
		if (mouseDeltaLength > 8.0f)
		{
			m_mouseDelta = glm::normalize(m_mouseDelta) * 8.0f;
		}
		m_mouseDelta.y = -m_mouseDelta.y;
		m_lastCursorPos = cursorPos;
	}
}

glm::vec3 Input::MoveInput() const
{
	return m_moveInput;
}

glm::vec2 Input::MouseDelta() const
{
	return m_mouseDelta;
}

bool Input::LookActive() const
{
	return m_lookActive;
}

bool Input::LookBecameActive() const
{
	return m_lookBecameActive;
}

float Input::DeltaTime() const
{
	return m_deltaTime;
}

bool Input::KeyDown(int key) const
{
	return m_window && m_windowFocused && glfwGetKey(m_window->GLFW(), key) == GLFW_PRESS;
}

bool Input::ControllerButtonDown(int button, int joystick) const
{
	if (!m_windowFocused || !glfwJoystickIsGamepad(joystick))
	{
		return false;
	}

	GLFWgamepadstate state{};
	return glfwGetGamepadState(joystick, &state) == GLFW_TRUE
		&& button >= 0
		&& button <= GLFW_GAMEPAD_BUTTON_LAST
		&& state.buttons[button] == GLFW_PRESS;
}

float Input::ControllerAxisValue(int axis, int joystick) const
{
	if (!m_windowFocused || !glfwJoystickIsGamepad(joystick))
	{
		return 0.0f;
	}

	GLFWgamepadstate state{};
	if (glfwGetGamepadState(joystick, &state) != GLFW_TRUE
		|| axis < 0
		|| axis > GLFW_GAMEPAD_AXIS_LAST)
	{
		return 0.0f;
	}

	const float value = state.axes[axis];
	const float deadzone = 0.15f;
	return std::abs(value) >= deadzone ? value : 0.0f;
}

bool Input::WindowFocused() const
{
	return m_windowFocused;
}

void Input::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
	if (input)
	{
		input->HandleMouseButton(button, action);
	}

	GLFWmousebuttonfun previous = input ? input->m_previousMouseButtonCallback : nullptr;
	if (previous)
	{
		previous(window, button, action, mods);
	}
}

void Input::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
	if (input)
	{
		input->HandleCursorPos(xpos, ypos);
	}

	GLFWcursorposfun previous = input ? input->m_previousCursorPosCallback : nullptr;
	if (previous)
	{
		previous(window, xpos, ypos);
	}
}

void Input::HandleMouseButton(int button, int action)
{
	if (!m_window || !Root::Current().State().IsGameMode())
	{
		return;
	}

	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(m_window->GLFW(), &x, &y);

	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			MyGUI::InputManager::getInstance().injectMousePress(static_cast<int>(x), static_cast<int>(y), MyGUI::MouseButton::Left);
		}
		else if (action == GLFW_RELEASE)
		{
			MyGUI::InputManager::getInstance().injectMouseRelease(static_cast<int>(x), static_cast<int>(y), MyGUI::MouseButton::Left);
		}
	}
}

void Input::HandleCursorPos(double xpos, double ypos)
{
	if (!m_window || !Root::Current().State().IsGameMode())
	{
		return;
	}

	MyGUI::InputManager::getInstance().injectMouseMove(
		static_cast<int>(xpos),
		static_cast<int>(ypos),
		0);
}

