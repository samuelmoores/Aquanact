#include "Engine/Core/Input.h"

#include "Engine/Core/Window.h"
#include "GLFW/glfw3.h"

#include <cmath>
#include <algorithm>

#include <MYGUI/MyGUI_InputManager.h>
#include <MYGUI/MyGUI_MouseButton.h>

// -----------------------------------------------------------------------------
// Cursor and active-device implementation helpers
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Lifecycle and frame processing
// -----------------------------------------------------------------------------
void Input::startUp(Window& window)
{
	// Bind the native window first. Every reset below, including the cursor
	// baseline and callback registration, depends on this handle being valid.
	m_window = &window;

	// Reset all transient state so a second editor/game session cannot inherit
	// look focus, device ownership, queued UI events, or a stale frame snapshot.
	m_frame = InputFrame{};
	m_context = InputContext::Editor;
	m_deltaTime = 0.0f;
	m_moveInput = glm::vec3(0.0f);
	m_mouseDelta = glm::vec2(0.0f);
	m_lookActive = false;
	m_lookBecameActive = false;
	m_gameplayFocusActive = false;
	m_revealCursorThisFrame = false;
	m_mouseCapturedByUI = false;
	m_windowFocused = false;
	m_lastRoutedMousePosition = glm::ivec2(0);
	m_lastWindowMousePosition = glm::ivec2(0);
	m_lastMouseRoutedToMyGUI = false;
	m_lastInjectedMouseMoves = 0;
	m_lastInjectedMousePresses = 0;
	m_lastInjectedMouseReleases = 0;
	m_pendingMouseEvents.clear();
	m_previousMouseButtonCallback = nullptr;
	m_previousCursorPosCallback = nullptr;

	// Establish the initial cursor baseline before callbacks can report motion.
	// Without this baseline, the first cursor event could look like a large
	// movement and incorrectly switch device ownership or move the camera.
	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(m_window->GLFW(), &x, &y);
	m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
	m_lastReportedCursorPos = m_lastCursorPos;
	m_mouseMoveSerial = 0;
	m_lastMouseMoveSerial = 0;
	m_mouseButtonActivityThisFrame = false;

	// Start each session with deterministic device/cursor state. The frame
	// router may change ownership on the first processed frame after startup.
	m_previousGamepadState = {};
	m_previousGamepadStateValid = false;
	if (ControllerConnected())
	{
		GLFWgamepadstate gamepadState{};
		if (glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState) == GLFW_TRUE)
		{
			m_previousGamepadState = gamepadState;
			m_previousGamepadStateValid = true;
		}
	}
	// A connected gamepad owns the initial menu cursor. Mouse activity can
	// transfer ownership back to mouse/keyboard in UpdateCursorMode().
	m_activeDevice = ControllerConnected()
		? ActiveInputDevice::Gamepad
		: ActiveInputDevice::MouseKeyboard;
	m_deviceChangedThisFrame = false;
	// Startup begins in the editor context, so the native cursor must remain
	// available. If a controller is already being used, the first menu-context
	// update will hide it through UpdateCursorMode.
	UnhideMouseCursor();

	// Install the user pointer before installing callbacks so callbacks always
	// resolve to a fully initialized Input instance. The previous callbacks are
	// retained so ImGui's GLFW backend continues receiving the same events.
	glfwSetWindowUserPointer(m_window->GLFW(), this);
	m_previousMouseButtonCallback = glfwSetMouseButtonCallback(m_window->GLFW(), &Input::MouseButtonCallback);
	m_previousCursorPosCallback = glfwSetCursorPosCallback(m_window->GLFW(), &Input::CursorPosCallback);
}

void Input::shutDown()
{
	if (m_window && (m_lookActive || glfwGetInputMode(m_window->GLFW(), GLFW_CURSOR) != GLFW_CURSOR_NORMAL)) {
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
	m_activeDevice = ActiveInputDevice::MouseKeyboard;
	m_deviceChangedThisFrame = false;
	m_mouseDelta = glm::vec2(0.0f);
	m_mouseButtonActivityThisFrame = false;
	m_pendingMouseEvents.clear();
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
	const bool gameMode = m_context != InputContext::Editor &&
		m_context != InputContext::GameGUIPreview;

	// These values are rebuilt every frame rather than accumulating input.
	m_moveInput = glm::vec3(0.0f);
	m_lookBecameActive = false;
	m_deviceChangedThisFrame = false;

	// Resolve cursor visibility once from the current game and focus state.
	UpdateCursorMode(gameMode);
	m_lastMouseMoveSerial = m_mouseMoveSerial;
	m_mouseButtonActivityThisFrame = false;

	// Do not process gameplay or camera input while the window is unfocused.
	if (!m_windowFocused)
	{
		m_mouseDelta = glm::vec2(0.0f);
		PublishFrame();
		return;
	}

	// Game UI may consume the mouse. When it does, skip the remaining input
	// processing for this frame.
	if (!UpdateGameLook(gameMode))
	{
		PublishFrame();
		return;
	}

	// Movement input is independent of camera look and is rebuilt each frame.
	UpdateMovement();
	UpdateEditorLook(gameMode);

	// Mouse delta is meaningful only while camera look is active.
	m_mouseDelta = glm::vec2(0.0f);
	if (!m_lookActive)
	{
		PublishFrame();
		return;
	}

	// Ignore the click-position delta when look mode is first activated, so the
	// camera does not jump from the cursor's pre-click position.
	if (m_ignoreMouseDeltaOnce)
	{
		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(m_window->GLFW(), &x, &y);
		m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
		m_ignoreMouseDeltaOnce = false;
		PublishFrame();
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
	PublishFrame();
}

// -----------------------------------------------------------------------------
// Context and ownership transitions
// -----------------------------------------------------------------------------
void Input::TransitionToContext(InputContext context)
{
	if (m_context == context)
	{
		return;
	}

	m_context = context;
	m_lookBecameActive = false;
	if (context == InputContext::Editor ||
		context == InputContext::GameGUIPreview ||
		context == InputContext::MainMenu ||
		context == InputContext::Paused)
	{
		ReleaseCursorFocus();
	}
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

void Input::ReleaseCursorForUI()
{
	ReleaseCursorFocus();
}

void Input::RevealCursorForFrame()
{
	m_revealCursorThisFrame = true;
	SetActiveDevice(ActiveInputDevice::MouseKeyboard);
	UnhideMouseCursor();
}

void Input::DispatchPendingMouseEvents(bool dispatchToMyGUI)
{
	m_lastMouseRoutedToMyGUI = dispatchToMyGUI;
	m_lastInjectedMouseMoves = 0;
	m_lastInjectedMousePresses = 0;
	m_lastInjectedMouseReleases = 0;
	if (!m_window)
	{
		return;
	}

	// Events are dispatched only after the frame-level router has selected the
	// MyGUI target. GLFW callbacks only collect events and never choose a UI.
	if (dispatchToMyGUI)
	{
		MyGUI::InputManager& manager = MyGUI::InputManager::getInstance();
		for (const PendingMouseEvent& event : m_pendingMouseEvents)
		{
			switch (event.type)
			{
			case PendingMouseEvent::Type::Move:
				manager.injectMouseMove(event.x, event.y, 0);
				++m_lastInjectedMouseMoves;
				break;
			case PendingMouseEvent::Type::Press:
				manager.injectMousePress(event.x, event.y, MyGUI::MouseButton::Left);
				++m_lastInjectedMousePresses;
				break;
			case PendingMouseEvent::Type::Release:
				manager.injectMouseRelease(event.x, event.y, MyGUI::MouseButton::Left);
				++m_lastInjectedMouseReleases;
				break;
			}
		}

		// Re-send the current cursor position every routed UI frame. This makes
		// hover state deterministic after a menu is loaded or the cursor mode is
		// restored, even when GLFW did not emit a new cursor-position callback.
		double cursorX = 0.0;
		double cursorY = 0.0;
		glfwGetCursorPos(m_window->GLFW(), &cursorX, &cursorY);
		m_lastWindowMousePosition = glm::ivec2(static_cast<int>(cursorX), static_cast<int>(cursorY));
		int windowWidth = 0;
		int windowHeight = 0;
		int framebufferWidth = 0;
		int framebufferHeight = 0;
		glfwGetWindowSize(m_window->GLFW(), &windowWidth, &windowHeight);
		glfwGetFramebufferSize(m_window->GLFW(), &framebufferWidth, &framebufferHeight);
		const int mouseX = windowWidth > 0
			? static_cast<int>(cursorX * framebufferWidth / windowWidth)
			: static_cast<int>(cursorX);
		const int mouseY = windowHeight > 0
			? static_cast<int>(cursorY * framebufferHeight / windowHeight)
			: static_cast<int>(cursorY);
		m_lastRoutedMousePosition = glm::ivec2(mouseX, mouseY);
		manager.injectMouseMove(mouseX, mouseY, 0);
		++m_lastInjectedMouseMoves;
	}
	m_pendingMouseEvents.clear();
}

// -----------------------------------------------------------------------------
// Device and raw input queries
// -----------------------------------------------------------------------------
bool Input::ControllerConnected(int joystick) const
{
	// Presence is intentionally separate from input activity. A mapped GLFW
	// gamepad owns the initial menu cursor; mouse activity can relinquish it.
	return glfwJoystickPresent(joystick) == GLFW_TRUE &&
		glfwJoystickIsGamepad(joystick) == GLFW_TRUE;
}

bool Input::KeyDown(int key) const
{
	return m_window && m_windowFocused && glfwGetKey(m_window->GLFW(), key) == GLFW_PRESS;
}

bool Input::MouseButtonDown(int button) const
{
	return m_window && m_windowFocused && glfwGetMouseButton(m_window->GLFW(), button) == GLFW_PRESS;
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

// -----------------------------------------------------------------------------
// Compatibility accessors for legacy consumers
// -----------------------------------------------------------------------------
glm::vec3 Input::MoveInput() const
{
	return m_frame.moveInput;
}

glm::vec2 Input::MouseDelta() const
{
	return m_frame.mouseDelta;
}

bool Input::LookActive() const
{
	return m_frame.lookActive;
}

bool Input::LookBecameActive() const
{
	return m_frame.lookBecameActive;
}

bool Input::WindowFocused() const
{
	return m_frame.windowFocused;
}

float Input::DeltaTime() const
{
	return m_frame.deltaTime;
}

// -----------------------------------------------------------------------------
// Native window callbacks
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Callback event collection and frame publication
// -----------------------------------------------------------------------------
void Input::HandleMouseButton(int button, int action)
{
	m_mouseButtonActivityThisFrame = true;
	if (!m_window || button != GLFW_MOUSE_BUTTON_LEFT)
	{
		return;
	}

	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(m_window->GLFW(), &x, &y);
	int windowWidth = 0;
	int windowHeight = 0;
	int framebufferWidth = 0;
	int framebufferHeight = 0;
	glfwGetWindowSize(m_window->GLFW(), &windowWidth, &windowHeight);
	glfwGetFramebufferSize(m_window->GLFW(), &framebufferWidth, &framebufferHeight);
	const int mouseX = windowWidth > 0 ? static_cast<int>(x * framebufferWidth / windowWidth) : static_cast<int>(x);
	const int mouseY = windowHeight > 0 ? static_cast<int>(y * framebufferHeight / windowHeight) : static_cast<int>(y);

	if (action == GLFW_PRESS)
	{
		m_pendingMouseEvents.push_back({ PendingMouseEvent::Type::Press, mouseX, mouseY });
	}
	else if (action == GLFW_RELEASE)
	{
		m_pendingMouseEvents.push_back({ PendingMouseEvent::Type::Release, mouseX, mouseY });
	}
}

void Input::HandleCursorPos(double xpos, double ypos)
{
	m_lastWindowMousePosition = glm::ivec2(static_cast<int>(xpos), static_cast<int>(ypos));
	const glm::vec2 cursorPos(static_cast<float>(xpos), static_cast<float>(ypos));
	const glm::vec2 movement = cursorPos - m_lastReportedCursorPos;
	if (std::abs(movement.x) > 0.01f || std::abs(movement.y) > 0.01f)
	{
		++m_mouseMoveSerial;
		m_lastReportedCursorPos = cursorPos;
	}
	if (!m_window)
	{
		return;
	}

	int windowWidth = 0;
	int windowHeight = 0;
	int framebufferWidth = 0;
	int framebufferHeight = 0;
	glfwGetWindowSize(m_window->GLFW(), &windowWidth, &windowHeight);
	glfwGetFramebufferSize(m_window->GLFW(), &framebufferWidth, &framebufferHeight);
	const int mouseX = windowWidth > 0 ? static_cast<int>(xpos * framebufferWidth / windowWidth) : static_cast<int>(xpos);
	const int mouseY = windowHeight > 0 ? static_cast<int>(ypos * framebufferHeight / windowHeight) : static_cast<int>(ypos);
	m_pendingMouseEvents.push_back({ PendingMouseEvent::Type::Move, mouseX, mouseY });
}

void Input::PublishFrame()
{
	m_frame.deltaTime = m_deltaTime;
	m_frame.moveInput = m_moveInput;
	m_frame.mouseDelta = m_mouseDelta;
	m_frame.lookActive = m_lookActive;
	m_frame.lookBecameActive = m_lookBecameActive;
	m_frame.windowFocused = m_windowFocused;
	m_frame.context = m_context;
	m_frame.activeDevice = m_activeDevice;
	m_frame.deviceChangedThisFrame = m_deviceChangedThisFrame;
}

// -----------------------------------------------------------------------------
// Cursor and active-device control
// -----------------------------------------------------------------------------
void Input::SetCursorMode(int mode)
{
	if (!m_window)
	{
		return;
	}

	// Avoid reapplying the mode every frame. Repeated cursor-mode changes can
	// generate synthetic cursor movement and look like physical mouse input.
	if (glfwGetInputMode(m_window->GLFW(), GLFW_CURSOR) != mode)
	{
		glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, mode);
	}
	m_cursorMode = mode;
}

void Input::HideMouseCursor()
{
	SetCursorMode(GLFW_CURSOR_HIDDEN);
}

void Input::CaptureMouseCursor()
{
	SetCursorMode(GLFW_CURSOR_DISABLED);
}

void Input::UnhideMouseCursor()
{
	SetCursorMode(GLFW_CURSOR_NORMAL);
}

void Input::ReleaseCursorFocus()
{
	m_gameplayFocusActive = false;
	m_lookActive = false;
	m_ignoreMouseDeltaOnce = false;
	// Releasing gameplay look must not briefly expose the cursor when a
	// connected controller owns input, especially during a Playing <-> Paused
	// transition. Stale gamepad ownership after a disconnect must never hide it.
	const bool controllerInUse = ControllerConnected() &&
		m_activeDevice == ActiveInputDevice::Gamepad;
	if (controllerInUse)
	{
		HideMouseCursor();
	}
	else
	{
		UnhideMouseCursor();
	}
}

void Input::UpdateCursorMode(bool gameMode)
{
	// Outside a focused game window, the Windows cursor must remain available.
	if (!gameMode || !m_windowFocused)
	{
		m_previousGamepadStateValid = false;
		SetActiveDevice(ActiveInputDevice::MouseKeyboard);
		UnhideMouseCursor();
		return;
	}
	if (m_revealCursorThisFrame)
	{
		m_revealCursorThisFrame = false;
	}

	const bool mainMenuActive = m_context == InputContext::MainMenu;
	const bool paused = m_context == InputContext::Paused;

	// Physical mouse activity and controller activity both participate in device
	// ownership. This applies during gameplay as well as menus, so a controller
	// press cannot leave the cursor visible after a previous mouse interaction.
	const bool mouseInUse = m_mouseMoveSerial != m_lastMouseMoveSerial;
	const bool mouseActivity = m_mouseButtonActivityThisFrame;

	const bool controllerConnected = ControllerConnected();
	bool controllerActivityDetected = false;
	if (controllerConnected)
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
		if (m_activeDevice == ActiveInputDevice::Gamepad)
		{
			SetActiveDevice(ActiveInputDevice::MouseKeyboard);
		}
	}

	if (controllerActivityDetected)
	{
		SetActiveDevice(ActiveInputDevice::Gamepad);
		// The first real controller event immediately transfers cursor ownership
		// to the gamepad. Mouse movement below can reclaim it on the same frame.
		HideMouseCursor();
	}
	if (mouseInUse || mouseActivity)
	{
		SetActiveDevice(ActiveInputDevice::MouseKeyboard);
	}

	const bool controllerInUse = controllerConnected &&
		m_activeDevice == ActiveInputDevice::Gamepad;

	// Menus always expose the mouse unless a connected controller currently
	// owns input. Any physical mouse movement or click above transfers ownership
	// back to mouse/keyboard and restores the cursor in this same update.
	if (mainMenuActive || paused)
	{
		if (controllerInUse)
		{
			HideMouseCursor();
		}
		else
		{
			UnhideMouseCursor();
		}
		return;
	}

	if (controllerInUse)
	{
		HideMouseCursor();
		return;
	}

	if (!m_gameplayFocusActive)
	{
		HideMouseCursor();
	}
	else
	{
		// Gameplay needs captured input, not merely an invisible cursor. Disabled
		// mode supplies continuous virtual cursor movement beyond window edges.
		CaptureMouseCursor();
	}
}

void Input::SetActiveDevice(ActiveInputDevice device)
{
	if (m_activeDevice != device)
	{
		m_activeDevice = device;
		m_deviceChangedThisFrame = true;
	}
}

// -----------------------------------------------------------------------------
// Gameplay/editor input stages
// -----------------------------------------------------------------------------
bool Input::UpdateGameLook(bool gameMode)
{
	// Game look starts on a mouse click unless the UI currently owns the mouse.
	const bool escapePressed = glfwGetKey(m_window->GLFW(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
	if (!gameMode) return true;
	const bool menuActive = m_context == InputContext::MainMenu ||
		m_context == InputContext::Paused;
	if (escapePressed) {
		ReleaseCursorFocus();
		return true;
	}
	if (m_lookActive) return true;
	if (m_mouseCapturedByUI) { m_mouseDelta = glm::vec2(0.0f); return false; }
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
