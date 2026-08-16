#include "Engine/UI/EntityStateMachineWindow.h"
#include "Engine/UI/EngineGuiWidgets.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/ProjectManager.h"

#include <algorithm>
#include <filesystem>
#include <cctype>

namespace
{
	std::string NormalizedBindableTypeName(const BindableMember& member)
	{
		std::string typeName = member.typeName;
		std::transform(typeName.begin(), typeName.end(), typeName.begin(),
			[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return typeName;
	}

	std::vector<BindableMember> EntityStateConditionMembers(const std::vector<BindableMember>& members)
	{
		std::vector<BindableMember> result;
		for (const BindableMember& member : members)
		{
			const std::string typeName = NormalizedBindableTypeName(member);
			if (typeName == "bool" || typeName == "int" || typeName == "float" || typeName == "double")
				result.push_back(member);
		}
		return result;
	}
}

void EntityStateMachineWindow::Open(EntityStateMachine& machine)
{
	m_machine = &machine;
	m_openRequested = true;
	m_popupOpenRequested = true;
}

void EntityStateMachineWindow::Close()
{
	m_openRequested = false;
	m_popupOpenRequested = false;
	m_popupOpen = false;
	m_machine = nullptr;
}

bool EntityStateMachineWindow::BeginPopup(EntityStateMachine& machine)
{
	if (m_machine != &machine)
		return false;

	if (m_popupOpenRequested)
	{
		ImGui::OpenPopup("State Machine##AquanactEntityStateMachine");
		m_popupOpenRequested = false;
	}

	m_popupOpen = true;
	ImGui::SetNextWindowSize(ImVec2(900.0f, 0.0f), ImGuiCond_FirstUseEver);
	const bool visible = ImGui::BeginPopupModal(
		"State Machine##AquanactEntityStateMachine", &m_popupOpen);
	if (!visible && m_openRequested
		&& !ImGui::IsPopupOpen("State Machine##AquanactEntityStateMachine"))
	{
		m_states.erase(&machine);
		Close();
	}
	return visible;
}

void EntityStateMachineWindow::EndPopup(EntityStateMachine& machine)
{
	ImGui::EndPopup();
	if (!m_popupOpen)
	{
		m_states.erase(&machine);
		Close();
	}
}

void EntityStateMachineWindow::Draw(EntityStateMachine& machine)
{
	EntityStateMachineUiState& ui = StateFor(machine);
	const std::vector<EntityStateBindingSource> bindingSources = BuildBindingSources(machine.Owner());
	const std::vector<EntityStateMachine::State> states = machine.States();
	const std::vector<EntityStateMachine::Transition> transitions = machine.Transitions();
	std::vector<std::string> animationNames;
	if (Entity* owner = machine.Owner())
	{
		if (const Mesh* mesh = owner->GetMesh())
		{
			animationNames.reserve(static_cast<std::size_t>(mesh->NumAnimations()));
			for (int i = 0; i < mesh->NumAnimations(); ++i)
				animationNames.push_back(mesh->GetAnimationSource(i));
		}
	}

	InitializeState(ui, states);
	if (!BeginPopup(machine))
		return;

	if (ImGui::Button("Add State"))
	{
		ui.editingStateIndex = -1;
		ui.addStatePopupInitialized = false;
		ui.editStatePopupRequested = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Create Transition"))
	{
		ui.editingTransitionIndex = -1;
		ui.addTransitionPopupInitialized = false;
		ImGui::OpenPopup("Edit Transition##AquanactEntityStateMachine");
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Sound Event"))
	{
		ui.selectedSoundEventIndex = -1;
		for (std::size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex)
		{
			const auto visibleState = ui.visibleStateTransitions.find(states[stateIndex].name);
			if (visibleState != ui.visibleStateTransitions.end() && visibleState->second)
			{
				ui.selectedSoundStateIndex = static_cast<int>(stateIndex);
				break;
			}
		}
		ImGui::OpenPopup("Add Sound Event##AquanactEntityStateMachine");
	}

	const bool canAddSoundEvent = ui.selectedSoundStateIndex >= 0
		&& ui.selectedSoundStateIndex < static_cast<int>(states.size())
		&& !ui.soundEventSoundPath.empty();
	if (ImGui::BeginPopupModal("Add Sound Event##AquanactEntityStateMachine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SetNextItemWidth(90.0f);
		ImGui::InputInt("Frame", &ui.soundEventFrame);
		ui.soundEventFrame = std::max(0, ui.soundEventFrame);
		ImGui::Checkbox("Random Sample", &ui.soundEventRandomSample);
		const EngineGuiWidgets::AssetFilePickerOptions soundOptions{
			EngineGuiWidgets::SourceAssetDirectory("assets/audio/sfx"),
			"audio/sfx/",
			ui.soundEventRandomSample ? std::vector<std::string>{} : std::vector<std::string>{ ".wav", ".mp3", ".ogg", ".flac" },
			ui.soundEventRandomSample,
			"<Select sound>" };
		EngineGuiWidgets::AssetFileCombo("Sound", ui.soundEventSoundPath, soundOptions);
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Volume", &ui.soundEventVolume, 0.0f, 100.0f);
		if (ImGui::Button("Create") && canAddSoundEvent)
		{
			const auto& soundState = states[static_cast<std::size_t>(ui.selectedSoundStateIndex)];
			const EntityStateMachine::SoundEvent event{ ui.soundEventSoundPath, static_cast<float>(ui.soundEventFrame), ui.soundEventVolume, ui.soundEventRandomSample };
			if (ui.selectedSoundEventIndex >= 0) machine.UpdateStateSoundEvent(soundState.name, static_cast<std::size_t>(ui.selectedSoundEventIndex), event);
			else machine.AddStateSoundEvent(soundState.name, event);
			const std::filesystem::path projectPath = Root::Current().Projects().CurrentProjectPath();
			if (!projectPath.empty()) Root::Current().Projects().SaveProject(projectPath, Root::Current().Scenes());
			ui.soundEventSoundPath.clear(); ui.soundEventFrame = 0; ui.soundEventRandomSample = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	DrawStateEditPopup(machine, ui, states, transitions, animationNames);
	DrawStateOverview(machine, states, ui);
	DrawSoundEventOverview(states, ui);
	DrawTransitionOverview(transitions, bindingSources, ui);
	DrawTransitionPopup(machine, states, bindingSources, ui);
	EndPopup(machine);
}

std::vector<EntityStateBindingSource> EntityStateMachineWindow::BuildBindingSources(Entity* owner)
{
	std::vector<EntityStateBindingSource> sources;
	if (!owner)
		return sources;

	std::vector<BindableMember> entityMembers = EntityStateConditionMembers(owner->GetBindableMembers());
	if (!entityMembers.empty())
		sources.push_back({ {}, "Entity", std::move(entityMembers) });

	for (Component* component : owner->Components())
	{
		if (!component)
			continue;
		std::vector<BindableMember> members = EntityStateConditionMembers(component->GetBindableMembers());
		if (!members.empty())
			sources.push_back({ component->Name(), component->Name(), std::move(members) });
	}
	return sources;
}

void EntityStateMachineWindow::DrawStateOverview(
	EntityStateMachine& machine,
	const std::vector<EntityStateMachine::State>& states,
	EntityStateMachineUiState& ui)
{
	ImGui::SeparatorText("Initial State");
	const char* initialState = machine.InitialState().empty()
		? "<none>"
		: machine.InitialState().c_str();
	EngineGuiWidgets::SetComboWidthToContents(initialState);
	if (ImGui::BeginCombo("##EntityStateInitialState", initialState))
	{
		for (const EntityStateMachine::State& state : states)
		{
			const bool selected = machine.InitialState() == state.name;
			if (ImGui::Selectable(state.name.c_str(), selected))
			{
				machine.SetInitialState(state.name);
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SeparatorText("States");
	if (states.empty())
	{
		ImGui::TextDisabled("No states configured.");
		return;
	}

	for (std::size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex)
	{
		const EntityStateMachine::State& state = states[stateIndex];
		ImGui::PushID(static_cast<int>(stateIndex));
		bool& showTransitions = ui.visibleStateTransitions[state.name];
		ImGui::Checkbox("##ShowTransitions", &showTransitions);
		ImGui::SameLine();
		ImGui::TextUnformatted(state.name.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Edit"))
		{
			CopyStateName(ui.stateEditName, sizeof(ui.stateEditName), state.name);
			CopyStateName(ui.stateEditAnimationName, sizeof(ui.stateEditAnimationName), state.animationName);
			ui.stateEditBlocksMovement = state.blocksMovement;
			ui.stateEditBlocksInput = state.blocksInput;
			ui.editingStateIndex = static_cast<int>(stateIndex);
			ui.addStatePopupInitialized = false;
			ui.editStatePopupRequested = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete"))
		{
			ui.visibleStateTransitions.erase(state.name);
			machine.RemoveState(stateIndex);
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}
}

void EntityStateMachineWindow::DrawTransitionOverview(
	const std::vector<EntityStateMachine::Transition>& transitions,
	const std::vector<EntityStateBindingSource>& bindingSources,
	EntityStateMachineUiState& ui)
{
	ImGui::SeparatorText("Transitions");
	if (ImGui::BeginTable("EntityStateTransitionDirectionTable", 2,
		ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Checkbox("incoming", &ui.showIncomingTransitions);
		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("outgoing", &ui.showOutgoingTransitions);
		ImGui::EndTable();
	}

	ImGui::BeginChild("EntityStateTransitionBox", ImVec2(ImGui::GetContentRegionAvail().x, 220.0f), true);
	if (ImGui::BeginTable("EntityStateTransitionTable", 2,
		ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("incoming", ImGuiTableColumnFlags_WidthStretch, 0.5f);
		ImGui::TableSetupColumn("outgoing", ImGuiTableColumnFlags_WidthStretch, 0.5f);
		ImGui::TableNextRow();
		for (int column = 0; column < 2; ++column)
		{
			ImGui::TableSetColumnIndex(column);
			for (std::size_t i = 0; i < transitions.size(); ++i)
			{
				const auto& transition = transitions[i];
				const bool visible = column == 0
					? ui.showIncomingTransitions && ui.visibleStateTransitions[transition.to]
					: ui.showOutgoingTransitions && ui.visibleStateTransitions[transition.from];
				if (!visible) continue;
				ImGui::PushID(static_cast<int>(i * 2 + column));
				ImGui::Text("%s -> %s", transition.from.c_str(), transition.to.c_str());
				ImGui::SameLine();
				const std::string key = transition.from + "->" + transition.to + "#" + std::to_string(i);
				bool& expanded = ui.expandedTransitionConditions[key];
				if (ImGui::SmallButton("Conditions")) expanded = !expanded;
				if (expanded)
				{
					const auto conditions = transition.conditions.empty() ? std::vector<EntityStateMachine::Condition>{ transition.condition } : transition.conditions;
					for (const auto& condition : conditions)
						ImGui::TextUnformatted(ConditionToText(condition, bindingSources).c_str());
				}
				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}
	ImGui::EndChild();
}

void EntityStateMachineWindow::DrawSoundEventOverview(
	const std::vector<EntityStateMachine::State>& states,
	const EntityStateMachineUiState& ui)
{
	ImGui::SeparatorText("Sound Events");
	for (const auto& state : states)
	{
		const auto visible = ui.visibleStateTransitions.find(state.name);
		if (visible == ui.visibleStateTransitions.end() || !visible->second)
			continue;

		ImGui::TextUnformatted(state.name.c_str());
		if (state.soundEvents.empty())
		{
			ImGui::TextDisabled("  No sound events");
			continue;
		}
		for (std::size_t i = 0; i < state.soundEvents.size(); ++i)
		{
			const auto& event = state.soundEvents[i];
			ImGui::PushID(static_cast<int>(i));
			const std::string name = std::filesystem::path(event.soundName).filename().string();
			ImGui::BulletText("Frame %.0f: %s (Volume %.0f)%s", event.frame, name.c_str(),
				event.volume, event.randomSample ? " [Random]" : "");
			ImGui::PopID();
		}
	}
}

void EntityStateMachineWindow::DrawTransitionEndpoints(
	const std::vector<EntityStateMachine::State>& states,
	EntityStateMachineUiState& ui)
{
	EngineGuiWidgets::SetComboWidthToContents(ui.transitionFromState[0] != '\0' ? ui.transitionFromState : "<from>");
	if (ImGui::BeginCombo("##TransitionFrom", ui.transitionFromState[0] != '\0' ? ui.transitionFromState : "<from>"))
	{
		for (const auto& state : states)
		{
			const bool selected = std::strcmp(ui.transitionFromState, state.name.c_str()) == 0;
			if (ImGui::Selectable(state.name.c_str(), selected)) CopyStateName(ui.transitionFromState, sizeof(ui.transitionFromState), state.name);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::TextUnformatted("->");
	ImGui::SameLine();
	EngineGuiWidgets::SetComboWidthToContents(ui.transitionToState[0] != '\0' ? ui.transitionToState : "<to>");
	if (ImGui::BeginCombo("##TransitionTo", ui.transitionToState[0] != '\0' ? ui.transitionToState : "<to>"))
	{
		for (const auto& state : states)
		{
			const bool selected = std::strcmp(ui.transitionToState, state.name.c_str()) == 0;
			if (ImGui::Selectable(state.name.c_str(), selected)) CopyStateName(ui.transitionToState, sizeof(ui.transitionToState), state.name);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::Checkbox("No interrupt", &ui.transitionWaitForCurrentStateComplete);
}

void EntityStateMachineWindow::DrawTransitionConditionControls(EntityStateMachineUiState& ui)
{
	ImGui::Separator();
	if (ImGui::Button("Add Condition"))
	{
		EntityStateMachine::Condition condition;
		condition.right.constantValue = 1.0f;
		ui.conditions.push_back(std::move(condition));
	}
	ImGui::SetNextItemWidth(120.0f);
	ImGui::InputFloat("Blend Seconds", &ui.transitionBlendSeconds, 0.0f, 0.0f, "%.2f");
}

bool EntityStateMachineWindow::DrawTransitionCommitControls(
	EntityStateMachine& machine, EntityStateMachineUiState& ui)
{
	if (ImGui::Button(ui.editingTransitionIndex >= 0 ? "Update" : "Create"))
	{
		if (ui.editingTransitionIndex >= 0)
		{
			machine.UpdateTransition(static_cast<std::size_t>(ui.editingTransitionIndex), ui.transitionFromState,
				ui.transitionToState, ui.transitionBlendSeconds,
				ui.transitionWaitForCurrentStateComplete, ui.conditions);
		}
		else
		{
			machine.AddTransition(ui.transitionFromState, ui.transitionToState, ui.transitionBlendSeconds,
				ui.transitionWaitForCurrentStateComplete, ui.conditions);
			ui.visibleStateTransitions[ui.transitionFromState] = true;
		}
		ui.editingTransitionIndex = -1;
		ui.addTransitionPopupInitialized = false;
		ui.transitionListNeedsRefresh = true;
		ImGui::CloseCurrentPopup();
		return true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ui.editingTransitionIndex = -1;
		ui.addTransitionPopupInitialized = false;
		ImGui::CloseCurrentPopup();
		return true;
	}
	return false;
}

bool EntityStateMachineWindow::DrawRemoveCondition(const EntityStateMachineUiState& ui)
{
	if (ui.conditions.size() <= 1 || !ImGui::SmallButton("Remove Condition"))
		return false;
	// The editor loop owns mutation of the condition vector.
	return true;
}

void EntityStateMachineWindow::DrawOperandEditor(
	const char* label, EntityStateMachine::Operand& operand,
	const std::vector<EntityStateBindingSource>& sources, bool booleanConstant,
	bool horizontal, bool drawType, bool drawValue)
{
	ImGui::PushID(label);
	if (label && label[0]) ImGui::TextUnformatted(label);
	const char* types[] = { "Constant", "Entity" };
	int type = static_cast<int>(operand.type);
	EngineGuiWidgets::SetComboWidthToContents(types[type]);
	if (drawType && ImGui::Combo("##OperandType", &type, types, IM_ARRAYSIZE(types)))
	{
		operand.type = static_cast<EntityStateMachine::OperandType>(type);
		if (operand.type == EntityStateMachine::OperandType::Binding && operand.memberName.empty() && !sources.empty() && !sources.front().members.empty())
		{
			operand.componentName = sources.front().componentName;
			operand.memberName = sources.front().members.front().name;
		}
	}
	if (!drawValue) { ImGui::PopID(); return; }
	if (operand.type == EntityStateMachine::OperandType::Constant)
	{
		if (booleanConstant)
		{
			const char* values[] = { "false", "true" };
			int value = operand.constantValue != 0.0f ? 1 : 0;
			EngineGuiWidgets::SetComboWidthToContents(values[value]);
			if (ImGui::Combo("##OperandValue", &value, values, IM_ARRAYSIZE(values))) operand.constantValue = static_cast<float>(value);
		}
		else ImGui::InputFloat("##OperandValue", &operand.constantValue, 0.0f, 0.0f, "%.3f");
		ImGui::PopID(); return;
	}
	const EntityStateBindingSource* source = nullptr;
	for (const auto& candidate : sources) if (candidate.componentName == operand.componentName) { source = &candidate; break; }
	EngineGuiWidgets::SetComboWidthToContents(source ? source->label.c_str() : "<select variable>");
	if (ImGui::BeginCombo("##OperandVariable", source ? source->label.c_str() : "<select variable>"))
	{
		for (const auto& candidate : sources) if (ImGui::Selectable(candidate.label.c_str(), source == &candidate))
		{
			operand.componentName = candidate.componentName;
			operand.memberName = candidate.members.empty() ? "" : candidate.members.front().name;
		}
		ImGui::EndCombo();
	}
	if (horizontal) ImGui::SameLine();
	EngineGuiWidgets::SetComboWidthToContents(operand.memberName.empty() ? "<select member>" : operand.memberName.c_str());
	if (source && ImGui::BeginCombo("##OperandMember", operand.memberName.empty() ? "<select member>" : operand.memberName.c_str()))
	{
		for (const auto& member : source->members) if (ImGui::Selectable(member.name.c_str(), member.name == operand.memberName)) operand.memberName = member.name;
		ImGui::EndCombo();
	}
	ImGui::PopID();
}

std::string EntityStateMachineWindow::ConditionToText(
	const EntityStateMachine::Condition& condition,
	const std::vector<EntityStateBindingSource>& bindingSources)
{
	const bool booleanCondition = IsBooleanCondition(condition, bindingSources);
	const auto operandText = [booleanCondition](const EntityStateMachine::Operand& operand)
	{
		if (booleanCondition && operand.type == EntityStateMachine::OperandType::Constant)
		{
			return std::string(operand.constantValue != 0.0f ? "true" : "false");
		}
		return EntityStateMachine::OperandToString(operand);
	};

	return operandText(condition.left) + " " +
		EntityStateMachine::ComparatorToString(condition.comparator) + " " +
		operandText(condition.right);
}

void EntityStateMachineWindow::DrawComparator(EntityStateMachine::Condition& condition, bool booleanCondition)
{
	const char* numeric[] = { "Equal", "Not Equal", "Greater", "Less", "Greater Equal", "Less Equal" };
	const char* boolean[] = { "Equal", "Not Equal" };
	const char** options = booleanCondition ? boolean : numeric;
	const int count = booleanCondition ? 2 : 6;
	int index = booleanCondition
		? condition.comparator == EntityStateMachine::Comparator::NotEqual ? 1 : 0
		: static_cast<int>(condition.comparator);
	if (booleanCondition && index > 1) index = 0;
	EngineGuiWidgets::SetComboWidthToContents(options[index]);
	if (ImGui::Combo("##Comparator", &index, options, count))
		condition.comparator = booleanCondition
			? index == 1 ? EntityStateMachine::Comparator::NotEqual : EntityStateMachine::Comparator::Equal
			: static_cast<EntityStateMachine::Comparator>(index);
}

bool EntityStateMachineWindow::IsBooleanCondition(
	const EntityStateMachine::Condition& condition,
	const std::vector<EntityStateBindingSource>& sources)
{
	const auto isBooleanBinding = [&sources](const EntityStateMachine::Operand& operand)
	{
		if (operand.type != EntityStateMachine::OperandType::Binding)
			return false;
		for (const EntityStateBindingSource& source : sources)
		{
			if (source.componentName != operand.componentName)
				continue;
			for (const BindableMember& member : source.members)
			{
				if (member.name != operand.memberName)
					continue;
				std::string typeName = member.typeName;
				std::transform(typeName.begin(), typeName.end(), typeName.begin(),
					[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
				return typeName == "bool" || typeName == "boolean";
			}
		}
		return false;
	};
	return isBooleanBinding(condition.left) || isBooleanBinding(condition.right);
}

bool EntityStateMachineWindow::DrawConditionEditor(
	EntityStateMachine::Condition& condition,
	const std::vector<EntityStateBindingSource>& bindingSources,
	bool booleanCondition,
	EntityStateMachineUiState& ui,
	std::size_t conditionIndex)
{
	if (booleanCondition
		&& condition.comparator != EntityStateMachine::Comparator::Equal
		&& condition.comparator != EntityStateMachine::Comparator::NotEqual)
	{
		condition.comparator = EntityStateMachine::Comparator::Equal;
	}

	ImGui::PushID(static_cast<int>(conditionIndex));
	if (conditionIndex > 0)
		ImGui::Separator();

	ImGui::BeginTable("ConditionOperandLayout", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::BeginGroup();
	ImGui::PushID("Left");
	DrawOperandEditor("", condition.left, bindingSources, booleanCondition, false, true, false);
	DrawOperandEditor("", condition.left, bindingSources, booleanCondition, false, false, true);
	ImGui::PopID();
	ImGui::EndGroup();

	ImGui::TableSetColumnIndex(1);
	ImGui::BeginGroup();
	ImGui::PushID("LeftValue");
	DrawComparator(condition, booleanCondition);
	ImGui::PopID();
	ImGui::EndGroup();

	ImGui::TableSetColumnIndex(2);
	ImGui::BeginGroup();
	ImGui::PushID("RightValue");
	DrawOperandEditor("", condition.right, bindingSources, booleanCondition, false, true, false);
	DrawOperandEditor("", condition.right, bindingSources, booleanCondition, false, false, true);
	ImGui::PopID();
	ImGui::EndGroup();
	ImGui::EndTable();

	ImGui::Separator();
	ImGui::TextUnformatted(ConditionToText(condition, bindingSources).c_str());
	ImGui::Separator();
	const bool removeRequested = DrawRemoveCondition(ui);
	ImGui::PopID();
	return removeRequested;
}

void EntityStateMachineWindow::DrawTransitionEditor(
	EntityStateMachine& machine,
	const std::vector<EntityStateMachine::State>& states,
	const std::vector<EntityStateBindingSource>& bindingSources,
	EntityStateMachineUiState& ui)
{
	if (!ui.addTransitionPopupInitialized)
	{
		if (!states.empty())
		{
			CopyStateName(ui.transitionFromState, sizeof(ui.transitionFromState), states.front().name);
			if (states.size() > 1)
				CopyStateName(ui.transitionToState, sizeof(ui.transitionToState), states[1].name);
		}
		ui.conditions.clear();
		EntityStateMachine::Condition defaultCondition;
		defaultCondition.right.constantValue = 1.0f;
		ui.conditions.push_back(std::move(defaultCondition));
		ui.transitionBlendSeconds = 0.25f;
		ui.transitionWaitForCurrentStateComplete = false;
		ui.addTransitionPopupInitialized = true;
	}

	DrawTransitionEndpoints(states, ui);
	ImGui::Separator();
	for (std::size_t conditionIndex = 0; conditionIndex < ui.conditions.size(); ++conditionIndex)
	{
		EntityStateMachine::Condition& condition = ui.conditions[conditionIndex];
		const bool booleanCondition = IsBooleanCondition(condition, bindingSources);
		if (DrawConditionEditor(condition, bindingSources, booleanCondition, ui, conditionIndex))
		{
			ui.conditions.erase(ui.conditions.begin() + static_cast<std::ptrdiff_t>(conditionIndex));
			break;
		}
	}

	DrawTransitionConditionControls(ui);
	DrawTransitionCommitControls(machine, ui);
}

void EntityStateMachineWindow::DrawTransitionPopup(
	EntityStateMachine& machine,
	const std::vector<EntityStateMachine::State>& states,
	const std::vector<EntityStateBindingSource>& bindingSources,
	EntityStateMachineUiState& ui)
{
	if (ui.editTransitionPopupRequested)
	{
		ImGui::OpenPopup("Edit Transition##AquanactEntityStateMachine");
		ui.editTransitionPopupRequested = false;
	}

	ImGui::SetNextWindowSize(ImVec2(720.0f, 0.0f), ImGuiCond_FirstUseEver);
	if (ImGui::BeginPopupModal("Edit Transition##AquanactEntityStateMachine", nullptr))
	{
		DrawTransitionEditor(machine, states, bindingSources, ui);
		ImGui::EndPopup();
	}
}

bool EntityStateMachineWindow::DrawStateEditPopup(
	EntityStateMachine& machine,
	EntityStateMachineUiState& ui,
	const std::vector<EntityStateMachine::State>& states,
	const std::vector<EntityStateMachine::Transition>& transitions,
	const std::vector<std::string>& animationNames)
{
	if (ui.editStatePopupRequested)
	{
		ImGui::OpenPopup("Edit State##AquanactEntityStateMachine");
		ui.editStatePopupRequested = false;
	}

	bool changed = false;
	if (!ImGui::BeginPopupModal("Edit State##AquanactEntityStateMachine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return changed;

	if (!ui.addStatePopupInitialized)
	{
		if (ui.editingStateIndex >= 0 && static_cast<std::size_t>(ui.editingStateIndex) < states.size())
		{
			const auto& state = states[static_cast<std::size_t>(ui.editingStateIndex)];
			CopyStateName(ui.stateEditName, sizeof(ui.stateEditName), state.name);
			CopyStateName(ui.stateEditAnimationName, sizeof(ui.stateEditAnimationName), state.animationName);
			ui.stateEditBlocksMovement = state.blocksMovement;
			ui.stateEditBlocksInput = state.blocksInput;
		}
		else
		{
			ui.stateEditName[0] = '\0';
			ui.stateEditAnimationName[0] = '\0';
			ui.stateEditBlocksMovement = false;
			ui.stateEditBlocksInput = false;
		}
		ui.addStatePopupInitialized = true;
	}

	ImGui::InputText("State Name", ui.stateEditName, sizeof(ui.stateEditName));
	DrawAnimationSelector(animationNames, ui.stateEditAnimationName, sizeof(ui.stateEditAnimationName));
	ImGui::Checkbox("Block Movement", &ui.stateEditBlocksMovement);
	ImGui::Checkbox("Blocks Input", &ui.stateEditBlocksInput);

	const bool editing = ui.editingStateIndex >= 0
		&& static_cast<std::size_t>(ui.editingStateIndex) < states.size();
	if (editing)
	{
		const EntityStateMachine::State& editedState =
			states[static_cast<std::size_t>(ui.editingStateIndex)];
		ImGui::SeparatorText("Transitions");
		bool hasTransitions = false;
		for (std::size_t transitionIndex = 0; transitionIndex < transitions.size(); ++transitionIndex)
		{
			const EntityStateMachine::Transition& transition = transitions[transitionIndex];
			if (transition.from != editedState.name && transition.to != editedState.name)
			{
				continue;
			}

			hasTransitions = true;
			ImGui::PushID(static_cast<int>(transitionIndex));
			ImGui::Text("%s -> %s", transition.from.c_str(), transition.to.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Edit"))
			{
				CopyStateName(ui.transitionFromState, sizeof(ui.transitionFromState), transition.from);
				CopyStateName(ui.transitionToState, sizeof(ui.transitionToState), transition.to);
				ui.transitionBlendSeconds = transition.blendSeconds;
				ui.transitionWaitForCurrentStateComplete = transition.waitForCurrentStateComplete;
				ui.conditions = transition.conditions.empty()
					? std::vector<EntityStateMachine::Condition>{ transition.condition }
					: transition.conditions;
				ui.editingTransitionIndex = static_cast<int>(transitionIndex);
				ui.addTransitionPopupInitialized = true;
				ui.editTransitionPopupRequested = true;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				const std::string key = transition.from + "->" + transition.to
					+ "#" + std::to_string(transitionIndex);
				ui.expandedTransitionConditions.erase(key);
				machine.RemoveTransition(transitionIndex);
				changed = true;
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		if (!hasTransitions)
		{
			ImGui::TextDisabled("No transitions for this state.");
		}
	}

	const bool validName = HasStateName(states, ui.stateEditName);
	const bool duplicateName = validName && (!editing || states[static_cast<std::size_t>(ui.editingStateIndex)].name != ui.stateEditName);
	ImGui::BeginDisabled(ui.stateEditName[0] == '\0' || duplicateName);
	if (ImGui::Button(editing ? "Update" : "Create"))
	{
		if (editing)
		{
			const std::string previousName = states[static_cast<std::size_t>(ui.editingStateIndex)].name;
			const bool visible = ui.visibleStateTransitions[previousName];
			if (machine.UpdateState(static_cast<std::size_t>(ui.editingStateIndex), ui.stateEditName,
				ui.stateEditAnimationName, ui.stateEditBlocksMovement, ui.stateEditBlocksInput))
			{
				ui.visibleStateTransitions.erase(previousName);
				ui.visibleStateTransitions[ui.stateEditName] = visible;
				changed = true;
			}
		}
		else
		{
			machine.AddState(ui.stateEditName, ui.stateEditAnimationName,
				ui.stateEditBlocksMovement, ui.stateEditBlocksInput);
			changed = true;
		}
		ui.editingStateIndex = -1;
		ui.addStatePopupInitialized = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ui.editingStateIndex = -1;
		ui.addStatePopupInitialized = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
	return changed;
}
