#include "Engine/UI/InputMapWindow.h"

#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace
{
	std::string InputBindingLabel(const InputBinding& binding)
	{
		if (binding.type == InputBindingType::Key)
		{
			if (const char* name = glfwGetKeyName(binding.code, 0)) return std::string("Key: ") + name;
			switch (binding.code)
			{
			case GLFW_KEY_SPACE: return "Key: Space"; case GLFW_KEY_ENTER: return "Key: Enter";
			case GLFW_KEY_ESCAPE: return "Key: Escape";
			case GLFW_KEY_LEFT: return "Key: Left"; case GLFW_KEY_RIGHT: return "Key: Right";
			case GLFW_KEY_UP: return "Key: Up"; case GLFW_KEY_DOWN: return "Key: Down";
			default: return "Key: " + std::to_string(binding.code);
			}
		}
		if (binding.type == InputBindingType::MouseButton)
		{
			switch (binding.code)
			{
			case GLFW_MOUSE_BUTTON_LEFT: return "Mouse: Left"; case GLFW_MOUSE_BUTTON_RIGHT: return "Mouse: Right";
			case GLFW_MOUSE_BUTTON_MIDDLE: return "Mouse: Middle"; default: return "Mouse: Button " + std::to_string(binding.code + 1);
			}
		}
		if (binding.type == InputBindingType::MouseDelta) return "Mouse";
		if (binding.type == InputBindingType::ControllerStick) return binding.stick == InputStick::Left ? "Left Stick" : "Right Stick";
		if (binding.type == InputBindingType::ControllerDigital)
		{
			switch (binding.code)
			{
			case GLFW_GAMEPAD_BUTTON_A: return "A"; case GLFW_GAMEPAD_BUTTON_B: return "B";
			case GLFW_GAMEPAD_BUTTON_X: return "X"; case GLFW_GAMEPAD_BUTTON_Y: return "Y";
			case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return "Left Bumper"; case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return "Right Bumper";
			case GLFW_GAMEPAD_BUTTON_BACK: return "Back"; case GLFW_GAMEPAD_BUTTON_START: return "Start";
			case GLFW_GAMEPAD_BUTTON_GUIDE: return "Guide";
			case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return "Left Stick Click";
			case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return "Right Stick Click";
			case GLFW_GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up"; case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
			case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left"; case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
			default: return "Controller digital: " + std::to_string(binding.code);
			}
		}
		return "Unknown binding";
	}
}

void InputMapWindow::Draw()
{
	if (!m_open)
		return;
	InputManager& inputManager = Root::Current().InputActions();
	InputMapUiState& ui = m_ui;

	// Find bindings by their purpose rather than their current button. This keeps
	// an edited direction associated with its row after its key changes.
	const auto findDirectionalBinding =
		[](std::vector<InputBinding>& bindings, InputBindingType type, const glm::vec2& direction) -> InputBinding*
	{
		for (InputBinding& binding : bindings)
		{
			if (binding.type == type && binding.vector == direction)
			{
				return &binding;
			}
		}
		return nullptr;
	};

	const auto findFirstBindingOfType =
		[](std::vector<InputBinding>& bindings, InputBindingType type) -> InputBinding*
	{
		for (InputBinding& binding : bindings)
		{
			if (binding.type == type)
			{
				return &binding;
			}
		}
		return nullptr;
	};

	// Keep Move first, then present every other runtime action alphabetically.
	std::vector<std::string> actionNames;
	actionNames.reserve(inputManager.Bindings().size());
	for (const auto& [actionName, bindings] : inputManager.Bindings())
	{
		(void)bindings;
		actionNames.push_back(actionName);
	}
	std::sort(actionNames.begin(), actionNames.end(), [](const std::string& left, const std::string& right)
	{
		if (left == right) return false;
		if (left == "Move") return true;
		if (right == "Move") return false;
		return left < right;
	});

	if (inputManager.Bindings().find(ui.selectedAction) == inputManager.Bindings().end())
	{
		ui.selectedAction = inputManager.Bindings().contains("Move")
			? "Move"
			: (actionNames.empty() ? std::string{} : actionNames.front());
	}

	bool open = m_open;
	ImGui::SetNextWindowSize(ImVec2(540.0f, 0.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Input Map", &open, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		// Action selection and creation
		ImGui::TextUnformatted("Mapping Action");
		ImGui::SameLine();
			// Keep the action selector compact so the creation button stays visible.
			ImGui::SetNextItemWidth(180.0f);
		if (ImGui::BeginCombo("##InputAction", ui.selectedAction.empty() ? "<select action>" : ui.selectedAction.c_str()))
		{
			for (const std::string& actionName : actionNames)
			{
				const bool selected = actionName == ui.selectedAction;
				if (ImGui::Selectable(actionName.c_str(), selected))
				{
					ui.selectedAction = actionName;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("Add New Action"))
		{
			ui.newActionName[0] = '\0';
			ui.statusMessage.clear();
			ui.addActionPopupRequested = true;
		}

		if (ui.addActionPopupRequested)
		{
			ImGui::OpenPopup("Add Input Action##AquanactInputMap");
			ui.addActionPopupRequested = false;
		}

		if (ImGui::BeginPopupModal("Add Input Action##AquanactInputMap", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Action Name");
			ImGui::SetNextItemWidth(280.0f);
			ImGui::InputText("##NewInputActionName", ui.newActionName, sizeof(ui.newActionName));

			if (ImGui::Button("Create"))
			{
				std::string actionName = ui.newActionName;
				const auto firstCharacter = std::find_if_not(actionName.begin(), actionName.end(), [](unsigned char ch) { return std::isspace(ch); });
				const auto lastCharacter = std::find_if_not(actionName.rbegin(), actionName.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
				actionName = firstCharacter < lastCharacter ? std::string(firstCharacter, lastCharacter) : std::string{};

				if (actionName.empty())
				{
					ui.statusMessage = "Enter an action name.";
				}
				else if (inputManager.Bindings().contains(actionName))
				{
					ui.statusMessage = "That action already exists.";
				}
				else
				{
					inputManager.SetBindings(actionName, {});
					ui.selectedAction = actionName;
					ui.statusMessage.clear();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				ui.statusMessage.clear();
				ImGui::CloseCurrentPopup();
			}
			EngineGuiWidgets::StatusMessage(ui.statusMessage);
			ImGui::EndPopup();
		}

		ImGui::Separator();

		const auto bindingIt = inputManager.Bindings().find(ui.selectedAction);
		if (bindingIt != inputManager.Bindings().end())
		{
			std::vector<InputBinding> editedBindings = bindingIt->second;
			bool bindingsChanged = false;

			// Shared keyboard selector for Move's four vector directions.
			const auto drawKeyboardDirection = [&](const char* label, int defaultKey, const glm::vec2& direction)
			{
				InputBinding* binding = findDirectionalBinding(editedBindings, InputBindingType::Key, direction);
				if (!binding)
				{
					editedBindings.push_back({ InputBindingType::Key, defaultKey, GLFW_JOYSTICK_1, 1.0f, direction });
					binding = &editedBindings.back();
					bindingsChanged = true;
				}

				if (ImGui::BeginCombo(label, InputBindingLabel(*binding).c_str()))
				{
					const int keyOptions[] = {
						GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
						GLFW_KEY_UP, GLFW_KEY_LEFT, GLFW_KEY_DOWN, GLFW_KEY_RIGHT
					};
					for (const int keyCode : keyOptions)
					{
						const InputBinding option{ InputBindingType::Key, keyCode, GLFW_JOYSTICK_1, 1.0f, direction };
						const bool selected = keyCode == binding->code;
						const std::string optionLabel = InputBindingLabel(option);
						if (ImGui::Selectable(optionLabel.c_str(), selected))
						{
							*binding = option;
							bindingsChanged = true;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			};

			ImGui::PushID(ui.selectedAction.c_str());
			if (ui.selectedAction == "Move")
			{
				EngineGuiWidgets::SectionHeading("Keyboard / Mouse");
				drawKeyboardDirection("Up##KeyboardUp", GLFW_KEY_W, glm::vec2(0.0f, 1.0f));
				drawKeyboardDirection("Down##KeyboardDown", GLFW_KEY_S, glm::vec2(0.0f, -1.0f));
				drawKeyboardDirection("Left##KeyboardLeft", GLFW_KEY_A, glm::vec2(-1.0f, 0.0f));
				drawKeyboardDirection("Right##KeyboardRight", GLFW_KEY_D, glm::vec2(1.0f, 0.0f));

				ImGui::Spacing();
				EngineGuiWidgets::SectionHeading("Controller");
				const char* controllerModes[] = { "Digital", "Analog" };
				int controllerMode = findFirstBindingOfType(editedBindings, InputBindingType::ControllerStick) ? 1 : 0;
				if (ImGui::Combo("##MoveControllerMode", &controllerMode, controllerModes, IM_ARRAYSIZE(controllerModes)))
				{
					editedBindings.erase(std::remove_if(editedBindings.begin(), editedBindings.end(), [](const InputBinding& binding)
					{
						return binding.type == InputBindingType::ControllerDigital || binding.type == InputBindingType::ControllerStick;
					}), editedBindings.end());
					if (controllerMode == 0)
					{
						editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, 1.0f) });
						editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, -1.0f) });
						editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(-1.0f, 0.0f) });
						editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) });
					}
					else
					{
						editedBindings.push_back({ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Left });
					}
					bindingsChanged = true;
				}

				if (controllerMode == 0)
				{
					const char* labels[] = { "Up", "Down", "Left", "Right" };
					const InputBinding defaults[] = {
						{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, 1.0f) },
						{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, -1.0f) },
						{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(-1.0f, 0.0f) },
						{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
					};
					for (int directionIndex = 0; directionIndex < IM_ARRAYSIZE(defaults); ++directionIndex)
					{
						InputBinding* binding = findDirectionalBinding(editedBindings, InputBindingType::ControllerDigital, defaults[directionIndex].vector);
						if (!binding)
						{
							editedBindings.push_back(defaults[directionIndex]);
							binding = &editedBindings.back();
							bindingsChanged = true;
						}
						if (ImGui::BeginCombo(labels[directionIndex], InputBindingLabel(*binding).c_str()))
						{
							for (const InputBinding& optionTemplate : defaults)
							{
								InputBinding option = optionTemplate;
								option.vector = defaults[directionIndex].vector;
								const bool selected = option.code == binding->code;
								const std::string optionLabel = InputBindingLabel(option);
								if (ImGui::Selectable(optionLabel.c_str(), selected)) { *binding = option; bindingsChanged = true; }
								if (selected) ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
				}
				else
				{
					InputBinding* stickBinding = findFirstBindingOfType(editedBindings, InputBindingType::ControllerStick);
					if (!stickBinding)
					{
						editedBindings.push_back({ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Left });
						stickBinding = &editedBindings.back();
						bindingsChanged = true;
					}

					if (ImGui::BeginCombo("Stick##MoveControllerStick", InputBindingLabel(*stickBinding).c_str()))
					{
						const InputStick stickOptions[] = { InputStick::Left, InputStick::Right };
						for (const InputStick stick : stickOptions)
						{
							const bool selected = stick == stickBinding->stick;
							const char* label = stick == InputStick::Left ? "Left Stick" : "Right Stick";
							if (ImGui::Selectable(label, selected))
							{
								stickBinding->stick = stick;
								bindingsChanged = true;
							}
							if (selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
			}
			else if (ui.selectedAction == "Look")
			{
				EngineGuiWidgets::SectionHeading("Keyboard / Mouse");
				ImGui::TextUnformatted("Mouse Movement");

				ImGui::Spacing();
				EngineGuiWidgets::SectionHeading("Controller");
				InputBinding* stickBinding = findFirstBindingOfType(editedBindings, InputBindingType::ControllerStick);
				if (!stickBinding)
				{
					editedBindings.push_back({ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Right });
					stickBinding = &editedBindings.back();
					bindingsChanged = true;
				}

				if (ImGui::BeginCombo("Stick##LookControllerStick", InputBindingLabel(*stickBinding).c_str()))
				{
					const InputStick stickOptions[] = { InputStick::Left, InputStick::Right };
					for (const InputStick stick : stickOptions)
					{
						const bool selected = stick == stickBinding->stick;
						const char* label = stick == InputStick::Left ? "Left Stick" : "Right Stick";
						if (ImGui::Selectable(label, selected))
						{
							stickBinding->stick = stick;
							bindingsChanged = true;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
			else
			{
				// New actions are digital and receive one desktop and one controller binding.
				EngineGuiWidgets::SectionHeading("Keyboard / Mouse");
				InputBinding* desktopBinding = nullptr;
				for (InputBinding& binding : editedBindings)
				{
					if (binding.type == InputBindingType::Key || binding.type == InputBindingType::MouseButton)
					{
						desktopBinding = &binding;
						break;
					}
				}
				if (!desktopBinding)
				{
					editedBindings.push_back({ InputBindingType::Key, GLFW_KEY_SPACE, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) });
					desktopBinding = &editedBindings.back();
					bindingsChanged = true;
				}

				if (ImGui::BeginCombo("Button##DesktopActionButton", InputBindingLabel(*desktopBinding).c_str()))
				{
					const InputBinding desktopOptions[] = {
						{ InputBindingType::Key, GLFW_KEY_SPACE, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_ENTER, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_ESCAPE, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_E, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_F, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_Q, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::MouseButton, GLFW_MOUSE_BUTTON_LEFT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::MouseButton, GLFW_MOUSE_BUTTON_RIGHT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::MouseButton, GLFW_MOUSE_BUTTON_MIDDLE, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
					};
					for (const InputBinding& option : desktopOptions)
					{
						const bool selected = option.type == desktopBinding->type && option.code == desktopBinding->code;
						const std::string optionLabel = InputBindingLabel(option);
						if (ImGui::Selectable(optionLabel.c_str(), selected))
						{
							*desktopBinding = option;
							bindingsChanged = true;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::Spacing();
				EngineGuiWidgets::SectionHeading("Controller");
				InputBinding* controllerBinding = findFirstBindingOfType(editedBindings, InputBindingType::ControllerDigital);
				if (!controllerBinding)
				{
					editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_A, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) });
					controllerBinding = &editedBindings.back();
					bindingsChanged = true;
				}

				if (ImGui::BeginCombo("Button##ControllerActionButton", InputBindingLabel(*controllerBinding).c_str()))
				{
					const int buttonOptions[] = {
						GLFW_GAMEPAD_BUTTON_A, GLFW_GAMEPAD_BUTTON_B, GLFW_GAMEPAD_BUTTON_X, GLFW_GAMEPAD_BUTTON_Y,
						GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,
						GLFW_GAMEPAD_BUTTON_BACK, GLFW_GAMEPAD_BUTTON_START, GLFW_GAMEPAD_BUTTON_GUIDE,
						GLFW_GAMEPAD_BUTTON_LEFT_THUMB, GLFW_GAMEPAD_BUTTON_RIGHT_THUMB,
						GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_GAMEPAD_BUTTON_DPAD_DOWN,
						GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT
					};
					for (const int buttonCode : buttonOptions)
					{
						const InputBinding option{ InputBindingType::ControllerDigital, buttonCode, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) };
						const bool selected = buttonCode == controllerBinding->code;
						const std::string optionLabel = InputBindingLabel(option);
						if (ImGui::Selectable(optionLabel.c_str(), selected))
						{
							*controllerBinding = option;
							bindingsChanged = true;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}

			ImGui::PopID();

			if (bindingsChanged)
			{
				inputManager.SetBindings(ui.selectedAction, std::move(editedBindings));
			}
		}
	}
	ImGui::End();
	m_open = open;
}
