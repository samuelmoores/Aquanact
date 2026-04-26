#pragma once
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

enum class Action {
	MoveForward,
	MoveBack,
	MoveLeft,
	MoveRight,
	Escape
};

struct ActionState {
	bool isDown       = false;
	bool justPressed  = false;
	bool justReleased = false;
};

class Object3D;
struct GLFWwindow;

class Input {
public:
	Input();
	void Loop();
	void SetObjects();

	bool IsDown(Action a) const;
	bool JustPressed(Action a) const;
	bool JustReleased(Action a) const;

private:
	void UpdateActionStates();

	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

	std::unordered_map<int, Action>    m_bindings;
	std::unordered_map<Action, ActionState> m_actions;

	glm::vec2 m_mouseLast     = glm::vec2(0);
	glm::vec2 m_mouseCurr     = glm::vec2(0);
	glm::vec3 m_moveDirection = glm::vec3(0);
	float m_currRot   = 0.0f;
	float m_nextRot   = 0.0f;
	bool  m_blendRot  = false;
	bool  m_move      = false;
	bool  m_wasMoving = false;
	bool  m_windowActive = true;

	std::vector<Object3D*> m_objects;
};
