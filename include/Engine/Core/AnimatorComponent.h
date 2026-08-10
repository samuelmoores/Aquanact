#pragma once

#include <memory>
#include <string>
#include <vector>

#include <assimp/scene.h>

#include "Engine/Core/Animator.h"
#include "Engine/Core/Component.h"
#include "Engine/Core/Mesh.h"

class Entity;

class AnimatorComponent : public Component {
public:
	// State machine types
	enum class Comparator {
		Equal,
		NotEqual,
		Greater,
		Less,
		GreaterEqual,
		LessEqual
	};

	enum class OperandType {
		Constant,
		Binding
	};

	struct Operand {
		OperandType type = OperandType::Constant;
		float constantValue = 0.0f;
		std::string componentName;
		std::string memberName;
	};

	struct Condition {
		Operand left;
		Comparator comparator = Comparator::Equal;
		Operand right;
	};

	struct State {
		std::string name;
		int clipIndex = -1;
	};

	struct Transition {
		std::string from;
		std::string to;
		float blendSeconds = 0.33f;
		Condition condition;
		std::vector<Condition> conditions;
	};

	// Construction
	AnimatorComponent(Mesh* mesh);

	// Component lifecycle
	const char* Name() const override;
	void startUp(Entity& owner) override;
	void FirstFrame(Entity& owner) override;
	void Update(Entity& owner, float dt) override;

	// Runtime state control
	void SetInitialState(const std::string& stateName);
	void SetDesiredState(const std::string& stateName);

	// State machine configuration
	bool AddState(std::string name, int clipIndex);
	bool AddTransition(std::string from, std::string to, float blendSeconds, Condition condition = {});
	bool AddTransition(std::string from, std::string to, float blendSeconds, std::vector<Condition> conditions);
	bool UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, Condition condition = {});
	bool UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, std::vector<Condition> conditions);
	bool RemoveTransition(std::size_t index);

	// Runtime and state machine inspection
	Animator* GetAnimator();
	const Animator* GetAnimator() const;
	const std::vector<State>& States() const;
	const std::vector<Transition>& Transitions() const;
	const std::vector<Condition>& Conditions(const Transition& transition) const;
	const std::string& InitialState() const;
	const std::string& CurrentState() const;
	const std::string& DesiredState() const;

	// Transition diagnostics
	std::string LastTransitionDebug() const;
	const std::string& LastTransitionFrom() const;
	const std::string& LastTransitionTo() const;
	const std::string& LastTransitionLeftOperandText() const;
	const std::string& LastTransitionRightOperandText() const;
	std::string LastTransitionComparatorText() const;
	float LastTransitionLeftValue() const;
	float LastTransitionRightValue() const;
	bool LastTransitionPassed() const;
	const std::string& LastResolvedTargetState() const;
	int LastResolvedTargetClipIndex() const;
	bool LastResolvedTargetFound() const;

	// Display formatting
	static const char* ComparatorToString(Comparator comparator);
	static std::string OperandToString(const Operand& operand);

private:
	// State activation and transition evaluation
	void StartInitialState();
	void ActivateState(const std::string& stateName);
	bool TransitionConditionPasses(const Transition& transition, const Entity& owner, float& leftValue, float& rightValue, bool& operandsResolved) const;
	bool ResolveOperand(const Operand& operand, const Entity& owner, float& value) const;
	bool Compare(float lhs, float rhs, Comparator comparator) const;

	// State machine lookup
	const Transition* FindTransition(const std::string& from, const std::string& to) const;
	const State* FindState(const std::string& name) const;

	// Animation runtime
	std::unique_ptr<Animator> m_animator;

	// State machine definition
	std::vector<State> m_states;
	std::vector<Transition> m_transitions;

	// Active state
	std::string m_initialState;
	std::string m_currentState;
	std::string m_desiredState;
	float m_transitionCooldown = 0.0f;

	// Transition diagnostics
	std::string m_lastTransitionDebug;
	std::string m_lastTransitionFrom;
	std::string m_lastTransitionTo;
	Comparator m_lastTransitionComparator = Comparator::Equal;
	std::string m_lastTransitionLeftOperandText;
	std::string m_lastTransitionRightOperandText;
	float m_lastTransitionLeftValue = 0.0f;
	float m_lastTransitionRightValue = 0.0f;
	bool m_lastTransitionPassed = false;
	std::string m_lastResolvedTargetState;
	int m_lastResolvedTargetClipIndex = -1;
	bool m_lastResolvedTargetFound = false;
};


