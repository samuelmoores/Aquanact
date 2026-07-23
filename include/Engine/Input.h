#pragma once

#include "glm/glm.hpp"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

class Window;
class EngineCamera;

class Input {
public:
	Input() = default;

	void startUp(Window& window);
	void shutDown();
	void Update();
	void AttachCamera(EngineCamera& camera);

	glm::vec3 MoveInput() const;
	glm::vec2 MouseDelta() const;
	bool LookActive() const;
	bool LookBecameActive() const;
	bool WindowFocused() const;
	float DeltaTime() const;

private:
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

	void HandleMouseButton(int button, int action);
	void HandleCursorPos(double xpos, double ypos);

	Window* m_window = nullptr;
	GLFWmousebuttonfun m_previousMouseButtonCallback = nullptr;
	GLFWcursorposfun m_previousCursorPosCallback = nullptr;
	bool m_lookActive = false;
	glm::vec2 m_lastCursorPos = glm::vec2(0.0f);
	glm::vec2 m_mouseDelta = glm::vec2(0.0f);
	glm::vec3 m_moveInput = glm::vec3(0.0f);
	float m_deltaTime = 0.0f;
	bool m_lookBecameActive = false;
	bool m_windowFocused = false;
	EngineCamera* m_camera = nullptr;
};
