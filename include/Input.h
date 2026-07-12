#pragma once

#include "glm/glm.hpp"

class Window;

class Input {
public:
	Input() = default;

	void startUp(Window& window);
	void shutDown();
	void Update();

	glm::vec3 MoveInput() const;
	glm::vec2 MouseDelta() const;
	bool LookActive() const;
	bool LookBecameActive() const;
	float DeltaTime() const;

private:
	Window* m_window = nullptr;
	bool m_lookActive = false;
	glm::vec2 m_lastCursorPos = glm::vec2(0.0f);
	glm::vec2 m_mouseDelta = glm::vec2(0.0f);
	glm::vec3 m_moveInput = glm::vec3(0.0f);
	float m_deltaTime = 0.0f;
	bool m_lookBecameActive = false;
};
