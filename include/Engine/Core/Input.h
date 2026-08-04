#pragma once

#include "glm/glm.hpp"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <cstdint>

class Window;

class Input {
public:
	Input() = default;

	void startUp(Window& window);
	void shutDown();
	void CaptureCursorForLevel();
	void Update();

	glm::vec3 MoveInput() const;
	glm::vec2 MouseDelta() const;
	bool LookActive() const;
	bool LookBecameActive() const;
	bool WindowFocused() const;
	bool ControllerConnected(int joystick = GLFW_JOYSTICK_1) const;
	std::uint64_t MouseActivitySerial() const;
	float DeltaTime() const;
	bool KeyDown(int key) const;
	bool ControllerButtonDown(int button, int joystick = GLFW_JOYSTICK_1) const;
	float ControllerAxisValue(int axis, int joystick = GLFW_JOYSTICK_1) const;

private:
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

	void HandleMouseButton(int button, int action);
	void HandleCursorPos(double xpos, double ypos);
	void SetCursorMode(int mode);
	void HideMouseCursor();
	void CaptureMouseCursor();
	void UnhideMouseCursor();
	void ReleaseCursorFocus();
	void UpdateCursorMode(bool gameMode);
	bool UpdateGameLook(bool gameMode);
	void UpdateEditorLook(bool gameMode);
	void UpdateMovement();

	Window* m_window = nullptr;
	GLFWmousebuttonfun m_previousMouseButtonCallback = nullptr;
	GLFWcursorposfun m_previousCursorPosCallback = nullptr;
	bool m_lookActive = false;
	bool m_ignoreMouseDeltaOnce = false;
	glm::vec2 m_lastCursorPos = glm::vec2(0.0f);
	glm::vec2 m_mouseDelta = glm::vec2(0.0f);
	glm::vec3 m_moveInput = glm::vec3(0.0f);
	float m_deltaTime = 0.0f;
	bool m_lookBecameActive = false;
	bool m_windowFocused = false;
	std::uint64_t m_mouseActivitySerial = 0;
	std::uint64_t m_mouseMoveSerial = 0;
	std::uint64_t m_lastMouseMoveSerial = 0;
	glm::vec2 m_lastReportedCursorPos = glm::vec2(0.0f);
	GLFWgamepadstate m_previousGamepadState{};
	bool m_previousGamepadStateValid = false;
	bool m_controllerActive = false;
	bool m_controllerCursorHidden = false;
	bool m_gameplayFocusActive = false;
	int m_cursorMode = GLFW_CURSOR_NORMAL;
};

