#include "Engine/Core/InputManager.h"

#include "Engine/Core/Input.h"

#include "GLFW/glfw3.h"

#include <algorithm>
#include <cmath>
#include <utility>

void InputManager::startUp(Input& input)
{
	m_input = &input;
	ResetToDefaults();
	EvaluateActions();
}

void InputManager::shutDown()
{
	m_input = nullptr;
	m_bindings.clear();
	m_states.clear();
	m_vectorStates.clear();
	m_vectorDeltaStates.clear();
	m_vectorRateStates.clear();
}

void InputManager::Update()
{
	EvaluateActions();
}

void InputManager::ResetToDefaults()
{
	m_bindings.clear();
	SetBindings("Move", {
		{ InputBindingType::Key, GLFW_KEY_W, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, 1.0f) },
		{ InputBindingType::Key, GLFW_KEY_S, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, -1.0f) },
		{ InputBindingType::Key, GLFW_KEY_A, GLFW_JOYSTICK_1, 1.0f, glm::vec2(-1.0f, 0.0f) },
		{ InputBindingType::Key, GLFW_KEY_D, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
		{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, 1.0f) },
		{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, -1.0f) },
		{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(-1.0f, 0.0f) },
		{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
		{ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Left },
	});
	SetBindings("Look", {
		{ InputBindingType::MouseDelta, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 1.0f) },
		{ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Right },
	});
}

void InputManager::Bind(const std::string& action, InputBinding binding)
{
	m_bindings[action].push_back(binding);
	m_states.try_emplace(action);
}

void InputManager::SetBindings(const std::string& action, std::vector<InputBinding> bindings)
{
	m_bindings[action] = std::move(bindings);
	m_states.try_emplace(action);
}

void InputManager::ClearBindings(const std::string& action)
{
	m_bindings[action].clear();
	m_states.try_emplace(action);
}

float InputManager::Value(const std::string& action) const
{
	const auto state = m_states.find(action);
	return state == m_states.end() ? 0.0f : state->second.value;
}

glm::vec2 InputManager::VectorValue(const std::string& action) const
{
	const auto state = m_vectorStates.find(action);
	return state == m_vectorStates.end() ? glm::vec2(0.0f) : state->second;
}

glm::vec2 InputManager::VectorDeltaValue(const std::string& action) const
{
	const auto state = m_vectorDeltaStates.find(action);
	return state == m_vectorDeltaStates.end() ? glm::vec2(0.0f) : state->second;
}

glm::vec2 InputManager::VectorRateValue(const std::string& action) const
{
	const auto state = m_vectorRateStates.find(action);
	return state == m_vectorRateStates.end() ? glm::vec2(0.0f) : state->second;
}

bool InputManager::IsDown(const std::string& action) const
{
	return Value(action) > 0.5f;
}

bool InputManager::WasPressed(const std::string& action) const
{
	const auto state = m_states.find(action);
	return state != m_states.end() && state->second.value > 0.5f && state->second.previousValue <= 0.5f;
}

bool InputManager::WasReleased(const std::string& action) const
{
	const auto state = m_states.find(action);
	return state != m_states.end() && state->second.value <= 0.5f && state->second.previousValue > 0.5f;
}

bool InputManager::IsBindingConnected(const InputBinding& binding) const
{
	if (binding.type == InputBindingType::Key)
	{
		return binding.code >= 0 && binding.code <= GLFW_KEY_LAST;
	}
	if (binding.type == InputBindingType::MouseButton)
	{
		return binding.code >= GLFW_MOUSE_BUTTON_1 && binding.code <= GLFW_MOUSE_BUTTON_LAST;
	}
	if (binding.type == InputBindingType::MouseDelta)
	{
		return true;
	}
	if (binding.type == InputBindingType::ControllerDigital || binding.type == InputBindingType::ControllerStick)
	{
		return m_input && binding.joystick >= GLFW_JOYSTICK_1 && binding.joystick <= GLFW_JOYSTICK_LAST
			&& glfwJoystickIsGamepad(binding.joystick) == GLFW_TRUE;
	}
	return false;
}

bool InputManager::IsBindingDown(const InputBinding& binding) const
{
	if (!m_input)
	{
		return false;
	}
	if (binding.type == InputBindingType::Key)
	{
		return m_input->KeyDown(binding.code);
	}
	if (binding.type == InputBindingType::MouseButton)
	{
		return m_input->MouseButtonDown(binding.code);
	}
	if (binding.type == InputBindingType::MouseDelta)
	{
		return false;
	}
	if (binding.type == InputBindingType::ControllerDigital)
	{
		return m_input->ControllerButtonDown(binding.code, binding.joystick);
	}
	if (binding.type == InputBindingType::ControllerStick)
	{
		const int xAxis = binding.stick == InputStick::Left ? GLFW_GAMEPAD_AXIS_LEFT_X : GLFW_GAMEPAD_AXIS_RIGHT_X;
		const int yAxis = binding.stick == InputStick::Left ? GLFW_GAMEPAD_AXIS_LEFT_Y : GLFW_GAMEPAD_AXIS_RIGHT_Y;
		const glm::vec2 stick(
			m_input->ControllerAxisValue(xAxis, binding.joystick),
			m_input->ControllerAxisValue(yAxis, binding.joystick));
		return glm::length(stick) > 0.15f;
	}
	return false;
}

void InputManager::EvaluateActions()
{
	if (m_suppressControllerInputUntilRelease)
	{
		bool controllerButtonHeld = m_input &&
			m_input->ControllerButtonDown(GLFW_GAMEPAD_BUTTON_A);
		for (const auto& [action, bindings] : m_bindings)
		{
			(void)action;
			for (const InputBinding& binding : bindings)
			{
				if (binding.type == InputBindingType::ControllerDigital && m_input &&
					m_input->ControllerButtonDown(binding.code, binding.joystick))
				{
					controllerButtonHeld = true;
					break;
				}
			}
			if (controllerButtonHeld)
			{
				break;
			}
		}
		if (!controllerButtonHeld)
		{
			m_suppressControllerInputUntilRelease = false;
		}
	}

	m_vectorStates.clear();
	m_vectorDeltaStates.clear();
	m_vectorRateStates.clear();
	const bool uiCapturesKeyboard = m_captureMask.keyboard;
	const bool uiCapturesMouseButtons = m_captureMask.mouseButtons;
	const bool uiCapturesMouseMotion = m_captureMask.mouseMotion;
	for (auto& [action, state] : m_states)
	{
		// Pause commands must remain available while a runtime UI or diagnostic
		// ImGui window owns ordinary keyboard/controller input. The gameplay loop
		// still decides which contexts are allowed to consume these commands.
		const bool isGlobalCommandAction = action == "Pause" || action == "TogglePause";
		state.previousValue = state.value;
		state.value = 0.0f;
		glm::vec2 vectorValue(0.0f);
		glm::vec2 deltaValue(0.0f);
		glm::vec2 rateValue(0.0f);

		const auto bindingIt = m_bindings.find(action);
		if (!m_input || bindingIt == m_bindings.end())
		{
			continue;
		}

		for (const InputBinding& binding : bindingIt->second)
		{
			if (binding.type == InputBindingType::Key)
			{
				if (uiCapturesKeyboard && !isGlobalCommandAction)
				{
					continue;
				}
				if (m_input->KeyDown(binding.code))
				{
					rateValue += binding.vector * binding.scale;
				}
			}
			else if (binding.type == InputBindingType::MouseButton)
			{
				if (uiCapturesMouseButtons)
				{
					continue;
				}
				if (m_input->MouseButtonDown(binding.code))
				{
					rateValue += binding.vector * binding.scale;
				}
			}
			else if (binding.type == InputBindingType::MouseDelta)
			{
				if (uiCapturesMouseMotion)
				{
					continue;
				}
				deltaValue += m_input->Frame().mouseDelta * binding.vector * binding.scale;
			}
			else if (binding.type == InputBindingType::ControllerDigital)
			{
				if (m_suppressControllerInputUntilRelease ||
					(m_captureMask.controller && !isGlobalCommandAction))
				{
					continue;
				}
				if (m_input->ControllerButtonDown(binding.code, binding.joystick))
				{
					rateValue += binding.vector * binding.scale;
				}
			}
			else if (binding.type == InputBindingType::ControllerStick)
			{
				if (m_suppressControllerInputUntilRelease ||
					(m_captureMask.controller && !isGlobalCommandAction))
				{
					continue;
				}
				const int xAxis = binding.stick == InputStick::Left ? GLFW_GAMEPAD_AXIS_LEFT_X : GLFW_GAMEPAD_AXIS_RIGHT_X;
				const int yAxis = binding.stick == InputStick::Left ? GLFW_GAMEPAD_AXIS_LEFT_Y : GLFW_GAMEPAD_AXIS_RIGHT_Y;
				glm::vec2 stick(
					m_input->ControllerAxisValue(xAxis, binding.joystick),
					m_input->ControllerAxisValue(yAxis, binding.joystick));
				stick.y = -stick.y;
				if (glm::length(stick) > 1.0f)
				{
					stick = glm::normalize(stick);
				}
				rateValue += stick * binding.scale;
			}
		}

		vectorValue = rateValue + deltaValue;
		state.value = glm::clamp(glm::length(vectorValue), 0.0f, 1.0f);
		m_vectorStates[action] = glm::clamp(vectorValue, glm::vec2(-1.0f), glm::vec2(1.0f));
		m_vectorDeltaStates[action] = deltaValue;
		m_vectorRateStates[action] = glm::clamp(
			rateValue, glm::vec2(-1.0f), glm::vec2(1.0f));
		state.value = std::clamp(state.value, 0.0f, 1.0f);
	}
}
