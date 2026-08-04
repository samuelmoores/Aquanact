#include "Engine/Core/Input.h"

#include "Engine/Core/Window.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/GameplayManager.h"
#include "GLFW/glfw3.h"

#include <cmath>
#include <algorithm>

#include <imgui.h>
#include <MYGUI/MyGUI_InputManager.h>
#include <MYGUI/MyGUI_MouseButton.h>

void Input::SetCursorMode(int mode)
{
	if (!m_window)
	{
		return;
	}

	// Always apply the native mode. ImGui or another GLFW client may have
	// changed it behind our cached value while a level was loading.
	glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, mode);
	m_cursorMode = mode;
}

void Input::HideMouseCursor()
{
	m_controllerCursorHidden = true;
	SetCursorMode(GLFW_CURSOR_HIDDEN);
}

void Input::CaptureMouseCursor()
{
	m_controllerCursorHidden = true;
	SetCursorMode(GLFW_CURSOR_DISABLED);
}

void Input::UnhideMouseCursor()
{
	m_controllerCursorHidden = false;
	SetCursorMode(GLFW_CURSOR_NORMAL);
}

void Input::startUp(Window& window)
{
	m_window = &window;
	m_lookActive = false;
	m_lookBecameActive = false;
	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(m_window->GLFW(), &x, &y);
	m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
	m_lastReportedCursorPos = m_lastCursorPos;
	m_mouseDelta = glm::vec2(0.0f);
	m_mouseActivitySerial = 0;
	m_mouseMoveSerial = 0;
	m_lastMouseMoveSerial = 0;
	m_previousGamepadState = {};
	m_previousGamepadStateValid = false;
	m_controllerActive = false;
	m_controllerCursorHidden = false;
	m_cursorMode = glfwGetInputMode(m_window->GLFW(), GLFW_CURSOR);

	glfwSetWindowUserPointer(m_window->GLFW(), this);
	// Chain GLFW callbacks so ImGui keeps receiving input while MyGUI gets the
	// same raw mouse events in game mode.
	m_previousMouseButtonCallback = glfwSetMouseButtonCallback(m_window->GLFW(), &Input::MouseButtonCallback);
	m_previousCursorPosCallback = glfwSetCursorPosCallback(m_window->GLFW(), &Input::CursorPosCallback);
}

void Input::shutDown()
{
	if (m_window && (m_lookActive || m_controllerCursorHidden)) {
		SetCursorMode(GLFW_CURSOR_NORMAL);
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
	m_previousGamepadState = {};
	m_previousGamepadStateValid = false;
	m_controllerActive = false;
	m_controllerCursorHidden = false;
	m_mouseDelta = glm::vec2(0.0f);
}

void Input::CaptureCursorForLevel()
{
	if (!m_window)
	{
		return;
	}

	// Level entry is an explicit gameplay boundary: reclaim keyboard/mouse
	// focus even when the level was launched from a GUI button or editor view.
	m_window->Focus();
	m_windowFocused = true;
	m_gameplayFocusActive = true;
	// Cursor capture and camera-look activation are separate states. Level entry
	// must enable both; otherwise UpdateGameLook() waits for a mouse click even
	// though the window is already focused and the cursor is disabled.
	m_lookActive = true;
	m_lookBecameActive = true;
	m_ignoreMouseDeltaOnce = true;
	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(m_window->GLFW(), &x, &y);
	m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
	CaptureMouseCursor();
}

void Input::ReleaseCursorFocus()
{
	m_gameplayFocusActive = false;
	m_lookActive = false;
	m_ignoreMouseDeltaOnce = false;
	UnhideMouseCursor();
}

void Input::Update()
{
	// Input cannot be updated until the window has been created.
	if (!m_window)
	{
		return;
	}

	// Calculate frame timing once for the rest of the engine.
	static double lastTime = glfwGetTime();
	const double now = glfwGetTime();
	// A debugger pause, focus transition, or slow frame must not turn into one
	// enormous simulation step and visibly teleport the player/camera.
	m_deltaTime = std::clamp(static_cast<float>(now - lastTime), 0.0f, 1.0f / 30.0f);
	lastTime = now;

	// Refresh focus before updating cursor mode. This lets the cursor restore
	// immediately when the window loses focus.
	m_windowFocused = glfwGetWindowAttrib(m_window->GLFW(), GLFW_FOCUSED) == GLFW_TRUE;
	const bool gameMode = Root::Current().State().IsGameMode();

	// These values are rebuilt every frame rather than accumulating input.
	m_moveInput = glm::vec3(0.0f);
	m_lookBecameActive = false;

	// Resolve cursor visibility once from the current game and focus state.
	UpdateCursorMode(gameMode);

	// Do not process gameplay or camera input while the window is unfocused.
	if (!m_windowFocused)
	{
		m_mouseDelta = glm::vec2(0.0f);
		return; 
	}

	// Save this frame's physical mouse movement so the next frame can detect
	// which device most recently became active.
	m_lastMouseMoveSerial = m_mouseMoveSerial;

	// Game UI may consume the mouse. When it does, skip the remaining input
	// processing for this frame.
	if (!UpdateGameLook(gameMode))
		return;

	// Movement input is independent of camera look and is rebuilt each frame.
	UpdateMovement();
	UpdateEditorLook(gameMode);

	// Mouse delta is meaningful only while camera look is active.
	m_mouseDelta = glm::vec2(0.0f);
	if (!m_lookActive)
		return;

	// Ignore the click-position delta when look mode is first activated, so the
	// camera does not jump from the cursor's pre-click position.
	if (m_ignoreMouseDeltaOnce)
	{
		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(m_window->GLFW(), &x, &y);
		m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
		m_ignoreMouseDeltaOnce = false;
		return;
	}

	// Convert the current cursor position into a clamped, inverted camera delta.
	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(m_window->GLFW(), &x, &y);
	const glm::vec2 cursorPos(static_cast<float>(x), static_cast<float>(y));
	m_mouseDelta = cursorPos - m_lastCursorPos;
	if (const float length = glm::length(m_mouseDelta); length > 8.0f)
	{
		m_mouseDelta = glm::normalize(m_mouseDelta) * 8.0f;
	}
	m_mouseDelta.y = -m_mouseDelta.y;
	m_lastCursorPos = cursorPos;
}

void Input::UpdateCursorMode(bool gameMode)
{
	// Outside a focused game window, the Windows cursor must remain available.
	if (!gameMode || !m_windowFocused)
	{
		m_previousGamepadStateValid = false;
		UnhideMouseCursor();
		return;
	}

	const GameplayManager::GameState state = Root::Current().Gameplay().State();
	const bool menuActive = state == GameplayManager::GameState::MainMenu ||
		state == GameplayManager::GameState::Paused;

	// Device switching is a menu-only behavior. Levels keep their cursor hidden
	// independently of which input device is active.
	if (!menuActive)
	{
		m_previousGamepadStateValid = false;
		if (!m_gameplayFocusActive)
		{
			UnhideMouseCursor();
			return;
		}
		// Gameplay needs captured input, not merely an invisible cursor. Disabled
		// mode supplies continuous virtual cursor movement beyond window edges.
		CaptureMouseCursor();
		return;
	}

	// Only physical mouse movement gives control back to the mouse. Mouse-button
	// activity and cursor-mode callbacks do not release controller ownership.
	const bool mouseInUse = m_mouseMoveSerial != m_lastMouseMoveSerial;

	// A controller becomes active on a new button press or meaningful axis
	// movement. Held inputs do not reclaim control after the mouse takes over.
	bool controllerActivityDetected = false;
	if (ControllerConnected())
	{
		GLFWgamepadstate gamepadState{};
		if (glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState) == GLFW_TRUE)
		{
			for (int button = 0; button <= GLFW_GAMEPAD_BUTTON_LAST; ++button)
			{
				const bool pressed = gamepadState.buttons[button] == GLFW_PRESS;
				const bool wasPressed = m_previousGamepadStateValid &&
					m_previousGamepadState.buttons[button] == GLFW_PRESS;
				controllerActivityDetected |= pressed && !wasPressed;
			}
			for (int axis = 0; axis <= GLFW_GAMEPAD_AXIS_LAST; ++axis)
			{
				const float value = gamepadState.axes[axis];
				const float previousValue = m_previousGamepadStateValid
					? m_previousGamepadState.axes[axis]
					: (axis >= GLFW_GAMEPAD_AXIS_LEFT_TRIGGER ? -1.0f : 0.0f);
				const bool trigger = axis >= GLFW_GAMEPAD_AXIS_LEFT_TRIGGER;
				const bool outsideDeadzone = trigger ? value > -0.75f : std::abs(value) > 0.25f;
				controllerActivityDetected |= outsideDeadzone && std::abs(value - previousValue) > 0.05f;
			}
			m_previousGamepadState = gamepadState;
			m_previousGamepadStateValid = true;
		}
		else
		{
			m_previousGamepadStateValid = false;
		}
	}
	else
	{
		m_previousGamepadStateValid = false;
	}

	if (controllerActivityDetected)
	{
		m_controllerActive = true;
	}
	if (mouseInUse)
	{
		m_controllerActive = false;
	}

	const bool controllerInUse = m_controllerActive;
	if (controllerInUse)
	{
		HideMouseCursor();
	}
	else
	{
		UnhideMouseCursor();
	}
}

bool Input::UpdateGameLook(bool gameMode)
{
	// Game look starts on a mouse click unless the UI currently owns the mouse.
	const bool escapePressed = glfwGetKey(m_window->GLFW(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
	if (!gameMode) return true;
	const auto state = Root::Current().Gameplay().State();
	const bool menuActive = state == GameplayManager::GameState::MainMenu || state == GameplayManager::GameState::Paused;
	if (escapePressed) {
		ReleaseCursorFocus();
		return true;
	}
	if (m_lookActive) return true;
	const ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse || ImGui::IsAnyItemActive()) { m_mouseDelta = glm::vec2(0.0f); return false; }
	const bool mouseClick = glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ||
		glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS ||
		glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
	if (mouseClick && !menuActive) {
		m_lookActive = m_lookBecameActive = true;
		m_ignoreMouseDeltaOnce = true;
		double x, y; glfwGetCursorPos(m_window->GLFW(), &x, &y);
		m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
	}
	return true;
}

void Input::UpdateEditorLook(bool gameMode)
{
	// The editor uses the right mouse button for camera look.
	if (gameMode) return;
	const bool rightDown = glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
	if (rightDown && !m_lookActive) {
		m_lookActive = m_lookBecameActive = true;
		m_ignoreMouseDeltaOnce = true;
		double x, y; glfwGetCursorPos(m_window->GLFW(), &x, &y);
		m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
	} else if (!rightDown && m_lookActive) {
		m_lookActive = false; m_ignoreMouseDeltaOnce = false;
	}
}

void Input::UpdateMovement()
{
	// Translate the six movement keys into the per-frame movement vector.
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_W) == GLFW_PRESS) m_moveInput.z += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_S) == GLFW_PRESS) m_moveInput.z -= 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_D) == GLFW_PRESS) m_moveInput.x += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_A) == GLFW_PRESS) m_moveInput.x -= 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_E) == GLFW_PRESS) m_moveInput.y += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_Q) == GLFW_PRESS) m_moveInput.y -= 1.0f;
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

bool Input::ControllerConnected(int joystick) const
{
	return glfwJoystickIsGamepad(joystick) == GLFW_TRUE;
}

std::uint64_t Input::MouseActivitySerial() const
{
	return m_mouseActivitySerial;
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
	++m_mouseActivitySerial;
	if (!m_window || !Root::Current().State().IsGameMode())
	{
		return;
	}
	if (ImGui::GetIO().WantCaptureMouse || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
	{
		return;
	}
	if (action == GLFW_PRESS && Root::Current().Gameplay().State() == GameplayManager::GameState::Playing && !m_gameplayFocusActive)
	{
		m_gameplayFocusActive = true;
		m_window->Focus();
		CaptureMouseCursor();
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
	const glm::vec2 cursorPos(static_cast<float>(xpos), static_cast<float>(ypos));
	const glm::vec2 movement = cursorPos - m_lastReportedCursorPos;
	if (std::abs(movement.x) > 0.01f || std::abs(movement.y) > 0.01f)
	{
		++m_mouseActivitySerial;
		++m_mouseMoveSerial;
		m_lastReportedCursorPos = cursorPos;
	}
	if (!m_window || !Root::Current().State().IsGameMode())
	{
		return;
	}

	MyGUI::InputManager::getInstance().injectMouseMove(
		static_cast<int>(xpos),
		static_cast<int>(ypos),
		0);
}

