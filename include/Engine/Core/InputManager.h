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
	MouseButton,
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

struct InputCaptureMask
{
	bool keyboard = false;
	bool mouseButtons = false;
	bool mouseMotion = false;
	bool controller = false;
};

class InputManager
{
public:
	// -------------------------------------------------------------------------
	// Lifecycle and frame processing
	// -------------------------------------------------------------------------
	void startUp(Input& input);
	void shutDown();
	void Update();

	// -------------------------------------------------------------------------
	// UI/gameplay capture policy
	// -------------------------------------------------------------------------
	void SetCaptureMask(InputCaptureMask mask) { m_captureMask = mask; }
	void SuppressControllerInputUntilRelease() { m_suppressControllerInputUntilRelease = true; }

	// -------------------------------------------------------------------------
	// Binding configuration
	// -------------------------------------------------------------------------
	void ResetToDefaults();
	void Bind(const std::string& action, InputBinding binding);
	void SetBindings(const std::string& action, std::vector<InputBinding> bindings);
	void ClearBindings(const std::string& action);
	const std::unordered_map<std::string, std::vector<InputBinding>>& Bindings() const { return m_bindings; }

	// -------------------------------------------------------------------------
	// Action value queries
	// -------------------------------------------------------------------------
	float Value(const std::string& action) const;
	glm::vec2 VectorValue(const std::string& action) const;

	// Delta values (for example mouse motion) are frame-local quantities and
	// must not be multiplied by dt. Rate values (sticks/keys/buttons) are held
	// inputs that callers normally integrate over time.
	glm::vec2 VectorDeltaValue(const std::string& action) const;
	glm::vec2 VectorRateValue(const std::string& action) const;
	bool IsDown(const std::string& action) const;
	bool WasPressed(const std::string& action) const;
	bool WasReleased(const std::string& action) const;

	// -------------------------------------------------------------------------
	// Binding state queries
	// -------------------------------------------------------------------------
	bool IsBindingConnected(const InputBinding& binding) const;
	bool IsBindingDown(const InputBinding& binding) const;

private:
	// -------------------------------------------------------------------------
	// Per-frame action evaluation
	// -------------------------------------------------------------------------
	void EvaluateActions();

	// -------------------------------------------------------------------------
	// Input source and binding configuration
	// -------------------------------------------------------------------------
	Input* m_input = nullptr;
	std::unordered_map<std::string, std::vector<InputBinding>> m_bindings;

	// -------------------------------------------------------------------------
	// Cached scalar and vector action state
	// -------------------------------------------------------------------------
	std::unordered_map<std::string, InputActionState> m_states;
	std::unordered_map<std::string, glm::vec2> m_vectorStates;
	std::unordered_map<std::string, glm::vec2> m_vectorDeltaStates;
	std::unordered_map<std::string, glm::vec2> m_vectorRateStates;

	// -------------------------------------------------------------------------
	// Current UI/gameplay capture policy
	// -------------------------------------------------------------------------
	InputCaptureMask m_captureMask;
	bool m_suppressControllerInputUntilRelease = false;
};
