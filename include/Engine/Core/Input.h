#pragma once

#include "glm/glm.hpp"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <cstdint>
#include <vector>

class Window;

class Input {
public:
	// High-level owner of the current input frame. The active context determines
	// whether input belongs to the editor, GameGUI preview, runtime menus, or
	// gameplay, and drives cursor behavior, device ownership, and UI capture.
	enum class InputContext
	{
		Editor,
		GameGUIPreview,
		MainMenu,
		Gameplay,
		Paused
	};

	enum class ActiveInputDevice
	{
		MouseKeyboard,
		Gamepad
	};

	struct InputFrame
	{
		float deltaTime = 0.0f;
		glm::vec3 moveInput = glm::vec3(0.0f);
		glm::vec2 mouseDelta = glm::vec2(0.0f);
		bool lookActive = false;
		bool lookBecameActive = false;
		bool windowFocused = false;
		InputContext context = InputContext::Editor;
		ActiveInputDevice activeDevice = ActiveInputDevice::MouseKeyboard;
		bool deviceChangedThisFrame = false;
	};

	Input() = default;

	// -------------------------------------------------------------------------
	// Lifecycle and frame processing
	// -------------------------------------------------------------------------
	void startUp(Window& window);
	void shutDown();
	void Update();

	// -------------------------------------------------------------------------
	// Context and ownership transitions
	// -------------------------------------------------------------------------
	void SetContext(InputContext context) { m_context = context; }
	void TransitionToContext(InputContext context);
	InputContext Context() const { return m_context; }
	void CaptureCursorForLevel();
	void ReleaseCursorForUI();
	void RevealCursorForFrame();
	void SetMouseCapturedByUI(bool captured) { m_mouseCapturedByUI = captured; }
	void DispatchPendingMouseEvents(bool dispatchToMyGUI);

	// -------------------------------------------------------------------------
	// Primary per-frame snapshot
	// -------------------------------------------------------------------------
	const InputFrame& Frame() const { return m_frame; }
	ActiveInputDevice ActiveDevice() const { return m_frame.activeDevice; }
	bool DeviceChangedThisFrame() const { return m_frame.deviceChangedThisFrame; }
	bool MouseCapturedByUI() const { return m_mouseCapturedByUI; }
	bool GameplayFocusActive() const { return m_gameplayFocusActive; }
	int CursorMode() const { return m_cursorMode; }
	std::size_t PendingMouseEventCount() const { return m_pendingMouseEvents.size(); }
	glm::ivec2 LastRoutedMousePosition() const { return m_lastRoutedMousePosition; }
	glm::ivec2 LastWindowMousePosition() const { return m_lastWindowMousePosition; }
	bool LastMouseRoutedToMyGUI() const { return m_lastMouseRoutedToMyGUI; }
	std::size_t LastInjectedMouseMoves() const { return m_lastInjectedMouseMoves; }
	std::size_t LastInjectedMousePresses() const { return m_lastInjectedMousePresses; }
	std::size_t LastInjectedMouseReleases() const { return m_lastInjectedMouseReleases; }

	// -------------------------------------------------------------------------
	// Device and raw input queries
	// -------------------------------------------------------------------------
	bool ControllerConnected(int joystick = GLFW_JOYSTICK_1) const;
	bool KeyDown(int key) const;
	bool MouseButtonDown(int button) const;
	bool ControllerButtonDown(int button, int joystick = GLFW_JOYSTICK_1) const;
	float ControllerAxisValue(int axis, int joystick = GLFW_JOYSTICK_1) const;

	// -------------------------------------------------------------------------
	// Compatibility accessors for legacy consumers
	// -------------------------------------------------------------------------
	glm::vec3 MoveInput() const;
	glm::vec2 MouseDelta() const;
	bool LookActive() const;
	bool LookBecameActive() const;
	bool WindowFocused() const;
	float DeltaTime() const;

private:
	// -------------------------------------------------------------------------
	// Native window callbacks
	// -------------------------------------------------------------------------
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

	// -------------------------------------------------------------------------
	// Callback event collection and frame publication
	// -------------------------------------------------------------------------
	void HandleMouseButton(int button, int action);
	void HandleCursorPos(double xpos, double ypos);
	void PublishFrame();

	// -------------------------------------------------------------------------
	// Cursor and active-device control
	// -------------------------------------------------------------------------
	void SetCursorMode(int mode);
	void HideMouseCursor();
	void CaptureMouseCursor();
	void UnhideMouseCursor();
	void ReleaseCursorFocus();
	void UpdateCursorMode(bool gameMode);
	void SetActiveDevice(ActiveInputDevice device);

	// -------------------------------------------------------------------------
	// Gameplay/editor input stages
	// -------------------------------------------------------------------------
	bool UpdateGameLook(bool gameMode);
	void UpdateEditorLook(bool gameMode);
	void UpdateMovement();

	// -------------------------------------------------------------------------
	// Window and native callback state
	// -------------------------------------------------------------------------
	Window* m_window = nullptr;
	GLFWmousebuttonfun m_previousMouseButtonCallback = nullptr;
	GLFWcursorposfun m_previousCursorPosCallback = nullptr;
	bool m_windowFocused = false;

	// -------------------------------------------------------------------------
	// Published frame and input-context state
	// -------------------------------------------------------------------------
	InputFrame m_frame;
	InputContext m_context = InputContext::Editor;
	float m_deltaTime = 0.0f;
	glm::vec3 m_moveInput = glm::vec3(0.0f);
	glm::vec2 m_mouseDelta = glm::vec2(0.0f);
	bool m_lookBecameActive = false;

	// -------------------------------------------------------------------------
	// Gameplay/editor look state
	// -------------------------------------------------------------------------
	bool m_lookActive = false;
	bool m_ignoreMouseDeltaOnce = false;
	glm::vec2 m_lastCursorPos = glm::vec2(0.0f);
	bool m_gameplayFocusActive = false;
	bool m_revealCursorThisFrame = false;

	// -------------------------------------------------------------------------
	// Mouse activity and callback tracking
	// -------------------------------------------------------------------------
	std::uint64_t m_mouseMoveSerial = 0;
	std::uint64_t m_lastMouseMoveSerial = 0;
	bool m_mouseButtonActivityThisFrame = false;
	glm::vec2 m_lastReportedCursorPos = glm::vec2(0.0f);

	// -------------------------------------------------------------------------
	// Controller/device ownership state
	// -------------------------------------------------------------------------
	GLFWgamepadstate m_previousGamepadState{};
	bool m_previousGamepadStateValid = false;
	ActiveInputDevice m_activeDevice = ActiveInputDevice::MouseKeyboard;
	bool m_deviceChangedThisFrame = false;

	// -------------------------------------------------------------------------
	// UI capture and cursor state
	// -------------------------------------------------------------------------
	bool m_mouseCapturedByUI = false;
	int m_cursorMode = GLFW_CURSOR_NORMAL;
	glm::ivec2 m_lastRoutedMousePosition = glm::ivec2(0);
	glm::ivec2 m_lastWindowMousePosition = glm::ivec2(0);
	bool m_lastMouseRoutedToMyGUI = false;
	std::size_t m_lastInjectedMouseMoves = 0;
	std::size_t m_lastInjectedMousePresses = 0;
	std::size_t m_lastInjectedMouseReleases = 0;

	// -------------------------------------------------------------------------
	// Queued mouse events for MyGUI routing
	// -------------------------------------------------------------------------
	struct PendingMouseEvent
	{
		enum class Type { Move, Press, Release };
		Type type;
		int x;
		int y;
	};

	std::vector<PendingMouseEvent> m_pendingMouseEvents;
};

