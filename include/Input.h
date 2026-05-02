#pragma once
#include <unordered_map>
#include <glm/glm.hpp>
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

enum class Action {
	MoveForward,
	MoveBack,
	MoveLeft,
	MoveRight,
	Escape,
	Interact
};

struct ActionState {
	bool isDown       = false;
	bool justPressed  = false;
	bool justReleased = false;
};

struct GLFWwindow;

class Input {
public:
	Input();
	void Loop();

	bool      IsDown(Action a) const;
	bool      JustPressed(Action a) const;
	bool      JustReleased(Action a) const;
	glm::vec2 MoveInput() const;

private:
	void UpdateActionStates();

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void CharCallback(GLFWwindow* window, unsigned int codepoint);
	static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
	static void CursorEnterCallback(GLFWwindow* window, int entered);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
#ifdef __EMSCRIPTEN__
	static EM_BOOL PointerLockChangeCallback(int eventType, const EmscriptenPointerlockChangeEvent* e, void* userData);
#endif

	std::unordered_map<int, Action>         m_bindings;
	std::unordered_map<Action, ActionState> m_actions;

	glm::vec2 m_mouseLast    = glm::vec2(0);
	glm::vec2 m_mouseCurr    = glm::vec2(0);
	bool      m_windowActive = true;
};
