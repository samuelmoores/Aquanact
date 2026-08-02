#pragma once

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include <string>
#include <unordered_map>
#include <vector>

class Input;

enum class InputBindingType
{
	Key,
	MouseDelta,
	ControllerDigital,
	ControllerStick,
};

enum class InputStick
{
	Left,
	Right,
};

struct InputBinding
{
	InputBindingType type = InputBindingType::Key;
	int code = 0;
	int joystick = GLFW_JOYSTICK_1;
	float scale = 1.0f;
	glm::vec2 vector = glm::vec2(0.0f);
	InputStick stick = InputStick::Left;
};

struct InputActionState
{
	float value = 0.0f;
	float previousValue = 0.0f;
};

class InputManager
{
public:
	void startUp(Input& input);
	void shutDown();
	void Update();

	void ResetToDefaults();
	void Bind(const std::string& action, InputBinding binding);
	void SetBindings(const std::string& action, std::vector<InputBinding> bindings);
	void ClearBindings(const std::string& action);

	float Value(const std::string& action) const;
	glm::vec2 VectorValue(const std::string& action) const;
	bool IsDown(const std::string& action) const;
	bool WasPressed(const std::string& action) const;
	bool WasReleased(const std::string& action) const;
	bool IsBindingConnected(const InputBinding& binding) const;
	bool IsBindingDown(const InputBinding& binding) const;
	const std::unordered_map<std::string, std::vector<InputBinding>>& Bindings() const { return m_bindings; }

private:
	void EvaluateActions();

	Input* m_input = nullptr;
	std::unordered_map<std::string, std::vector<InputBinding>> m_bindings;
	std::unordered_map<std::string, InputActionState> m_states;
	std::unordered_map<std::string, glm::vec2> m_vectorStates;
};
