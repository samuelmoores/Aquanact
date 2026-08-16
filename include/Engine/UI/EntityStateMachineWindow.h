#pragma once

#include "Engine/Core/EntityStateMachine.h"
#include <imgui.h>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstddef>

class Entity;

struct EntityStateBindingSource
{
	std::string componentName;
	std::string label;
	std::vector<BindableMember> members;
};

struct EntityStateMachineUiState
{
	bool initialized = false;
	char transitionFromState[64] = "";
	char transitionToState[64] = "";
	char transitionFilterFromState[64] = "";
	char transitionFilterToState[64] = "";
	std::map<std::string, bool> visibleStateTransitions;
	bool showIncomingTransitions = true;
	bool showOutgoingTransitions = true;
	float transitionBlendSeconds = 0.25f;
	bool transitionWaitForCurrentStateComplete = false;
	bool addStatePopupInitialized = false;
	bool editStatePopupRequested = false;
	int editingStateIndex = -1;
	char stateEditName[64] = "";
	char stateEditAnimationName[64] = "";
	bool stateEditBlocksMovement = false;
	bool stateEditBlocksInput = false;
	int selectedSoundEventIndex = -1;
	int soundEventFrame = 0;
	float soundEventVolume = 100.0f;
	std::string soundEventSoundPath;
	bool soundEventRandomSample = false;
	int selectedSoundStateIndex = -1;
	bool addTransitionPopupInitialized = false;
	bool editTransitionPopupRequested = false;
	bool transitionListNeedsRefresh = false;
	int editingTransitionIndex = -1;
	std::map<std::string, bool> expandedTransitionConditions;
	std::vector<EntityStateMachine::Condition> conditions;
};

class EntityStateMachineWindow
{
public:
	// Draws the Edit State popup. Returns true when state-dependent UI should
	// refresh after the popup commits a change.
	bool DrawStateEditPopup(
		EntityStateMachine& machine,
		EntityStateMachineUiState& ui,
		const std::vector<EntityStateMachine::State>& states,
		const std::vector<EntityStateMachine::Transition>& transitions,
		const std::vector<std::string>& animationNames);
	void DrawTransitionOverview(
		const std::vector<EntityStateMachine::Transition>& transitions,
		const std::vector<EntityStateBindingSource>& bindingSources,
		EntityStateMachineUiState& ui);
	void DrawSoundEventOverview(
		const std::vector<EntityStateMachine::State>& states,
		const EntityStateMachineUiState& ui);
	void DrawStateOverview(
		EntityStateMachine& machine,
		const std::vector<EntityStateMachine::State>& states,
		EntityStateMachineUiState& ui);
	void DrawTransitionEndpoints(
		const std::vector<EntityStateMachine::State>& states,
		EntityStateMachineUiState& ui);
	void DrawTransitionConditionControls(EntityStateMachineUiState& ui);
	bool DrawTransitionCommitControls(EntityStateMachine& machine, EntityStateMachineUiState& ui);
	void DrawTransitionEditor(EntityStateMachine& machine,
		const std::vector<EntityStateMachine::State>& states,
		const std::vector<EntityStateBindingSource>& bindingSources,
		EntityStateMachineUiState& ui);
	void DrawTransitionPopup(EntityStateMachine& machine,
		const std::vector<EntityStateMachine::State>& states,
		const std::vector<EntityStateBindingSource>& bindingSources,
		EntityStateMachineUiState& ui);
	bool DrawRemoveCondition(const EntityStateMachineUiState& ui);
	bool DrawConditionEditor(EntityStateMachine::Condition& condition,
		const std::vector<EntityStateBindingSource>& bindingSources,
		bool booleanCondition,
		EntityStateMachineUiState& ui,
		std::size_t conditionIndex);
	static void DrawOperandEditor(const char* label, EntityStateMachine::Operand& operand,
		const std::vector<EntityStateBindingSource>& sources, bool booleanConstant = false,
		bool horizontal = false, bool drawType = true, bool drawValue = true);
	static std::string ConditionToText(
		const EntityStateMachine::Condition& condition,
		const std::vector<EntityStateBindingSource>& bindingSources);
	static void DrawComparator(EntityStateMachine::Condition& condition, bool booleanCondition);
	static bool IsBooleanCondition(const EntityStateMachine::Condition& condition,
		const std::vector<EntityStateBindingSource>& sources);
	static std::vector<EntityStateBindingSource> BuildBindingSources(Entity* owner);

	bool BeginPopup(EntityStateMachine& machine);
	void EndPopup(EntityStateMachine& machine);
	void Draw(EntityStateMachine& machine);

	static void CopyStateName(char* destination, std::size_t destinationSize, const std::string& value)
	{
		std::strncpy(destination, value.c_str(), destinationSize - 1);
		destination[destinationSize - 1] = '\0';
	}

	static bool HasStateName(const std::vector<EntityStateMachine::State>& states, const char* name)
	{
		if (!name || name[0] == '\0')
			return false;
		for (const EntityStateMachine::State& state : states)
			if (state.name == name)
				return true;
		return false;
	}

	static bool DrawAnimationSelector(
		const std::vector<std::string>& animationNames,
		char* selectedAnimation,
		std::size_t selectedAnimationSize)
	{
		const char* preview = selectedAnimation[0] != '\0' ? selectedAnimation : "<none>";
		bool changed = false;
		if (ImGui::BeginCombo("Animation", preview))
		{
			for (const std::string& animationName : animationNames)
			{
				const bool selected = std::strcmp(selectedAnimation, animationName.c_str()) == 0;
				if (ImGui::Selectable(animationName.c_str(), selected))
				{
					CopyStateName(selectedAnimation, selectedAnimationSize, animationName);
					changed = true;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	void InitializeState(EntityStateMachineUiState& ui, const std::vector<EntityStateMachine::State>& states) const
	{
		if (!ui.initialized)
		{
			ui.stateEditName[0] = '\0';
			ui.transitionFromState[0] = '\0';
			ui.transitionToState[0] = '\0';
			ui.transitionFilterFromState[0] = '\0';
			ui.transitionFilterToState[0] = '\0';
			ui.visibleStateTransitions.clear();
			ui.showIncomingTransitions = true;
			ui.showOutgoingTransitions = true;
			ui.expandedTransitionConditions.clear();
			ui.initialized = true;
		}

		auto copyName = [](char* destination, std::size_t size, const std::string& value)
		{
			std::strncpy(destination, value.c_str(), size - 1);
			destination[size - 1] = '\0';
		};
		if (ui.stateEditName[0] == '\0' && !states.empty())
			copyName(ui.stateEditName, sizeof(ui.stateEditName), states.front().name);
		if (ui.transitionFromState[0] == '\0' && !states.empty())
			copyName(ui.transitionFromState, sizeof(ui.transitionFromState), states.front().name);
		if (ui.transitionToState[0] == '\0' && states.size() > 1)
			copyName(ui.transitionToState, sizeof(ui.transitionToState), states[1].name);
	}

	void Open(EntityStateMachine& machine);
	void Close();
	bool OpenRequested() const { return m_openRequested; }
	EntityStateMachineUiState& StateFor(EntityStateMachine& machine) { return m_states[&machine]; }
	void Forget(EntityStateMachine& machine) { m_states.erase(&machine); }

private:
	using StateMap = std::unordered_map<EntityStateMachine*, EntityStateMachineUiState>;
	StateMap m_states;
	bool m_openRequested = false;
	bool m_popupOpenRequested = false;
	bool m_popupOpen = true;
	EntityStateMachine* m_machine = nullptr;
};
