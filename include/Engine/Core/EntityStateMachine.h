#pragma once

#include <memory>
#include <string>
#include <vector>

#include <assimp/scene.h>

#include "Engine/Core/Animator.h"
#include "Engine/Core/Component.h"
#include "Engine/Core/Mesh.h"

class Entity;

// EntityStateMachine owns the runtime animation state machine for an entity.
// It loads animation clips from the entity mesh, exposes named states and
// transitions, and drives playback by evaluating conditions each frame.
class EntityStateMachine : public Component {
public:
	// Comparison operators used by transition conditions.
	enum class Comparator {
		Equal,
		NotEqual,
		Greater,
		Less,
		GreaterEqual,
		LessEqual
	};

	// Operand values can be constants or bound to a component member.
	enum class OperandType {
		Constant,
		Binding
	};

	// A single resolved value in a transition condition.
	struct Operand {
		OperandType type = OperandType::Constant;
		float constantValue = 0.0f;
		std::string componentName;
		std::string memberName;
	};

	// A comparison between two operands.
	struct Condition {
		Operand left;
		Comparator comparator = Comparator::Equal;
		Operand right;
	};

	// A sound scheduled against a frame in the state's animation clip.
	// Playback is intentionally handled separately from this data model.
	struct SoundEvent {
		std::string soundName;
		float frame = 0.0f;
		float volume = 100.0f;
		bool randomSample = false;
	};

	// A named animation state owned by the machine.
	struct State {
		std::string name;
		std::string animationName;
		bool blocksMovement = false;
		bool blocksInput = false;
		std::vector<SoundEvent> soundEvents;
	};

	// A transition between states. Multiple conditions are supported.
	struct Transition {
		std::string from;
		std::string to;
		float blendSeconds = 0.33f;
		bool waitForCurrentStateComplete = false;
		Condition condition;
		std::vector<Condition> conditions;
	};

	// Construction and lifecycle.
	EntityStateMachine(Mesh* mesh);

	const char* Name() const override;
	void startUp(Entity& owner) override;
	void FirstFrame(Entity& owner) override;
	void Update(Entity& owner, float dt) override;
	// Advances only the selected animation clip for editor visualization. State
	// transitions remain a gameplay concern and are not evaluated in this path.
	void UpdateEditorPreview(float dt);

	// State editing.
	void SetInitialState(const std::string& stateName);
	void SetDesiredState(const std::string& stateName);
	bool AddState(std::string name, std::string animationName = {}, bool blocksMovement = false, bool blocksInput = false);
	bool UpdateState(std::size_t index, std::string name, std::string animationName = {}, bool blocksMovement = false, bool blocksInput = false);
	bool RemoveState(std::size_t index);
	bool AddStateSoundEvent(const std::string& stateName, SoundEvent event);
	bool UpdateStateSoundEvent(const std::string& stateName, std::size_t eventIndex, SoundEvent event);
	bool RemoveStateSoundEvent(const std::string& stateName, std::size_t eventIndex);

	// Transition editing.
	bool AddTransition(std::string from, std::string to, float blendSeconds, bool waitForCurrentStateComplete, Condition condition = {});
	bool AddTransition(std::string from, std::string to, float blendSeconds, bool waitForCurrentStateComplete, std::vector<Condition> conditions);
	bool UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, bool waitForCurrentStateComplete, Condition condition = {});
	bool UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, bool waitForCurrentStateComplete, std::vector<Condition> conditions);
	bool RemoveTransition(std::size_t index);

	// Runtime accessors.
	Animator* GetAnimator();
	const Animator* GetAnimator() const;
	const std::vector<std::string>& AnimationNames() const;
	int FindAnimationIndex(const std::string& animationName) const;
	const std::vector<State>& States() const;
	const std::vector<Transition>& Transitions() const;
	const std::vector<Condition>& Conditions(const Transition& transition) const;
	const std::string& InitialState() const;
	const std::string& CurrentState() const;
	const std::string& DesiredState() const;
	bool CurrentStateLockedUntilComplete() const;
	bool CurrentStateWaitsForCompletion() const;
	float CurrentStateElapsedSeconds() const;
	float CurrentStateClipDurationSeconds() const;
	float CurrentStateSecondsUntilUnlock() const;
	bool CurrentStateBlocksMovement() const;
	bool CurrentStateBlocksInput() const;

	// Debug and inspection data for the UI.
	std::string LastTransitionDebug() const;
	const std::string& LastTransitionFrom() const;
	const std::string& LastTransitionTo() const;
	const std::string& LastTransitionLeftOperandText() const;
	const std::string& LastTransitionRightOperandText() const;
	std::string LastTransitionComparatorText() const;
	float LastTransitionLeftValue() const;
	float LastTransitionRightValue() const;
	bool LastTransitionPassed() const;
	bool LastTransitionWaitBlocked() const;
	const std::string& LastTransitionBlockedReason() const;
	const std::string& LastResolvedTargetState() const;
	int LastResolvedTargetClipIndex() const;
	bool LastResolvedTargetFound() const;

	// String helpers for UI and diagnostics.
	static const char* ComparatorToString(Comparator comparator);
	static std::string OperandToString(const Operand& operand);

private:
	// Internal state management.
	void ResetTransitionDiagnostics();
	void UpdateTimers(float dt);
	bool ProcessDesiredStateChange();
	void EvaluateTransitions(Entity& owner);
	bool EvaluateTransitionConditions(const Transition& transition, const Entity& owner, float& leftValue, float& rightValue, bool& operandsResolved);
	bool FireTransition(const Transition& transition);
	void StartInitialState(bool playSoundEvents = true);
	void ActivateState(const std::string& stateName);
	void PlaySoundEventsAtStateStart();
	void PlaySoundEvent(const SoundEvent& event);
	void PlaySoundEventsCrossed(float previousTicks, float currentTicks, float duration);
	int ResolveAnimationClipIndex(const State& state) const;
	bool TransitionConditionPasses(const Transition& transition, const Entity& owner, float& leftValue, float& rightValue, bool& operandsResolved) const;
	bool ResolveOperand(const Operand& operand, const Entity& owner, float& value) const;
	bool Compare(float lhs, float rhs, Comparator comparator) const;
	bool CurrentStateCanTransitionOut() const;

	// Lookups over the owned state and transition tables.
	const Transition* FindTransition(const std::string& from, const std::string& to) const;
	const State* FindState(const std::string& name) const;
	const State* FindCurrentState() const;

	// Owned runtime data, grouped by how the update loop and editor use it.

	// Core animation driver. Without this, the machine is inert.
	std::unique_ptr<Animator> m_animator;

	// Active state selection. These drive which clip is playing and what the
	// machine is trying to reach next.
	std::string m_currentState;
	std::string m_desiredState;
	std::string m_initialState;

	// State table and transition table. These define the machine's behavior.
	std::vector<State> m_states;
	std::vector<Transition> m_transitions;

	// Animation source names pulled from the mesh. Used to resolve state clips.
	std::vector<std::string> m_animationNames;

	// Timing and transition gating. These gate when transitions are allowed.
	float m_transitionCooldown = 0.0f;
	float m_currentStateElapsed = 0.0f;
	bool m_currentStateLockedUntilComplete = false;

	// Last comparator used by the UI when inspecting a transition.
	Comparator m_lastTransitionComparator = Comparator::Equal;

	// Transition debug snapshot. The editor reads this to explain what happened
	// on the last evaluation pass.
	std::string m_lastTransitionDebug;
	std::string m_lastTransitionFrom;
	std::string m_lastTransitionTo;
	std::string m_lastTransitionLeftOperandText;
	std::string m_lastTransitionRightOperandText;
	float m_lastTransitionLeftValue = 0.0f;
	float m_lastTransitionRightValue = 0.0f;
	bool m_lastTransitionPassed = false;
	bool m_lastTransitionWaitBlocked = false;
	std::string m_lastTransitionBlockedReason;

	// Last resolved target state/clip. Useful when a transition or forced state
	// change succeeds or fails.
	std::string m_lastResolvedTargetState;
	int m_lastResolvedTargetClipIndex = -1;
	bool m_lastResolvedTargetFound = false;
};
