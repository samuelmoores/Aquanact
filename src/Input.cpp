#include "Input.h"

#include "Window.h"
#include "EngineCamera.h"
#include "GLFW/glfw3.h"

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
}

void Input::shutDown()
{
	if (m_window && m_lookActive) {
		glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	m_window = nullptr;
	m_lookActive = false;
	m_lookBecameActive = false;
	m_mouseDelta = glm::vec2(0.0f);
}

void Input::AttachCamera(EngineCamera& camera)
{
	m_camera = &camera;
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
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_W) == GLFW_PRESS) m_moveInput.z += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_S) == GLFW_PRESS) m_moveInput.z -= 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_D) == GLFW_PRESS) m_moveInput.x += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_A) == GLFW_PRESS) m_moveInput.x -= 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_E) == GLFW_PRESS) m_moveInput.y += 1.0f;
	if (glfwGetKey(m_window->GLFW(), GLFW_KEY_Q) == GLFW_PRESS) m_moveInput.y -= 1.0f;

	const bool rightDown = glfwGetMouseButton(m_window->GLFW(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
	if (rightDown && !m_lookActive) {
		m_lookActive = true;
		m_lookBecameActive = true;
		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(m_window->GLFW(), &x, &y);
		m_lastCursorPos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
		glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	} else if (!rightDown && m_lookActive) {
		m_lookActive = false;
		glfwSetInputMode(m_window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	m_mouseDelta = glm::vec2(0.0f);
	if (m_lookActive) {
		double x = 0.0;
		double y = 0.0;
		glfwGetCursorPos(m_window->GLFW(), &x, &y);
		const glm::vec2 cursorPos(static_cast<float>(x), static_cast<float>(y));
		m_mouseDelta = cursorPos - m_lastCursorPos;
		m_lastCursorPos = cursorPos;
	}

	if (m_camera) {
		m_camera->UpdateFly(*this);
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

bool Input::WindowFocused() const
{
	return m_windowFocused;
}
