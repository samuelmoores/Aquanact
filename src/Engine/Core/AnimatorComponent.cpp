#include "Engine/Core/AnimatorComponent.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Animation.h"
#include "Engine/Core/Controller.h"
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace {
	std::string MakeStateNameFromSource(const std::string& sourcePath, int clipIndex)
	{
		std::filesystem::path p(sourcePath);
		std::string name = p.stem().string();
	if (name.empty())
	{
		name = "Clip";
	}
	return name;
}
}

AnimatorComponent::AnimatorComponent(Mesh* mesh)
{
	if (!mesh || !mesh->Skinned())
	{
		return;
	}

	m_animator = std::make_unique<Animator>(mesh->GetRootNode(), mesh->GetSkeletonPtr());
	for (int i = 0; i < mesh->NumAnimations(); ++i)
	{
		m_animator->AddClip(new Animation(mesh->GetAnimation(i)));
	}

	if (mesh->NumAnimations() > 0)
	{
		for (int i = 0; i < mesh->NumAnimations(); ++i)
		{
			AddState(MakeStateNameFromSource(mesh->GetAnimationSource(i), i), i);
		}
		m_initialState = m_states.front().name;
		m_currentState.clear();
		m_desiredState.clear();
	}
}

const char* AnimatorComponent::ComparatorToString(Comparator comparator)
{
	switch (comparator)
	{
	case Comparator::NotEqual: return "!=";
	case Comparator::Greater: return ">";
	case Comparator::Less: return "<";
	case Comparator::GreaterEqual: return ">=";
	case Comparator::LessEqual: return "<=";
	case Comparator::Equal:
	default:
		return "==";
	}
}

std::string AnimatorComponent::OperandToString(const Operand& operand)
{
	if (operand.type == OperandType::Constant)
	{
		std::ostringstream value;
		value << operand.constantValue;
		return value.str();
	}

	const std::string sourceName = operand.componentName.empty() ? "Entity" : operand.componentName;
	return sourceName + "." + (operand.memberName.empty() ? "<unbound>" : operand.memberName);
}

const char* AnimatorComponent::Name() const
{
	return "Animator";
}

void AnimatorComponent::startUp(Entity&)
{
}

void AnimatorComponent::FirstFrame(Entity&)
{
	if (!m_animator || m_states.empty())
	{
		return;
	}

	const std::string& stateName = !m_initialState.empty() ? m_initialState : m_states.front().name;
	SetInitialState(stateName);
}

void AnimatorComponent::Update(Entity& owner, float dt)
{
	m_lastTransitionDebug.clear();
	m_lastTransitionFrom.clear();
	m_lastTransitionTo.clear();
	m_lastTransitionLeftOperandText.clear();
	m_lastTransitionRightOperandText.clear();
	m_lastTransitionLeftValue = 0.0f;
	m_lastTransitionRightValue = 0.0f;
	m_lastTransitionPassed = false;
	m_lastResolvedTargetState.clear();
	m_lastResolvedTargetClipIndex = -1;
	m_lastResolvedTargetFound = false;
	if (m_transitionCooldown > 0.0f)
	{
		m_transitionCooldown -= dt;
		if (m_transitionCooldown < 0.0f)
		{
			m_transitionCooldown = 0.0f;
		}
	}
	if (m_animator)
	{
		if (!m_desiredState.empty() && m_desiredState != m_currentState)
		{
			const State* target = FindState(m_desiredState);
			if (target && target->clipIndex >= 0)
			{
				float blendSeconds = 0.25f;
				if (const Transition* transition = FindTransition(m_currentState, m_desiredState))
				{
					blendSeconds = transition->blendSeconds;
				}
				m_lastTransitionDebug = "Forced state change to " + m_desiredState + " using blend " + std::to_string(blendSeconds);
				m_animator->Play(target->clipIndex, blendSeconds);
				ActivateState(m_desiredState);
			}
		}
		else
		{
			if (m_transitionCooldown <= 0.0f)
			{
				for (const auto& transition : m_transitions)
				{
					if (transition.from != m_currentState)
					{
						continue;
					}

					float leftValue = 0.0f;
					float rightValue = 0.0f;
					bool operandsResolved = true;
					bool passed = true;
					const std::vector<Condition>& conditions = transition.conditions.empty()
						? std::vector<Condition>{ transition.condition }
						: transition.conditions;
					for (const Condition& condition : conditions)
					{
						float conditionLeftValue = 0.0f;
						float conditionRightValue = 0.0f;
						const bool leftResolved = ResolveOperand(condition.left, owner, conditionLeftValue);
						const bool rightResolved = ResolveOperand(condition.right, owner, conditionRightValue);
						const bool conditionResolved = leftResolved && rightResolved;
						operandsResolved = operandsResolved && conditionResolved;
						passed = passed && conditionResolved && Compare(conditionLeftValue, conditionRightValue, condition.comparator);
						if (&condition == &conditions.front())
						{
							leftValue = conditionLeftValue;
							rightValue = conditionRightValue;
							m_lastTransitionLeftOperandText = OperandToString(condition.left);
							m_lastTransitionRightOperandText = OperandToString(condition.right);
							m_lastTransitionComparator = condition.comparator;
						}
					}
					m_lastTransitionFrom = transition.from;
					m_lastTransitionTo = transition.to;
					m_lastTransitionLeftValue = leftValue;
					m_lastTransitionRightValue = rightValue;
					m_lastTransitionPassed = passed;
					if (!passed)
					{
						m_lastTransitionDebug = operandsResolved
							? "Checked " + transition.from + " -> " + transition.to + " and condition failed: " +
								m_lastTransitionLeftOperandText + " " + ComparatorToString(transition.condition.comparator) + " " +
								m_lastTransitionRightOperandText
							: "Checked " + transition.from + " -> " + transition.to + " but a condition binding could not be resolved";
						continue;
					}

					const State* target = FindState(transition.to);
					if (target && target->clipIndex >= 0)
					{
						m_lastTransitionDebug = "Transition fired: " + transition.from + " -> " + transition.to + " because " +
							m_lastTransitionLeftOperandText + " " + ComparatorToString(transition.condition.comparator) + " " +
							m_lastTransitionRightOperandText;
						m_lastResolvedTargetState = target->name;
						m_lastResolvedTargetClipIndex = target->clipIndex;
						m_lastResolvedTargetFound = true;
						m_animator->Play(target->clipIndex, transition.blendSeconds);
						ActivateState(transition.to);
						m_transitionCooldown = 0.15f;
					}
					else
					{
						m_lastResolvedTargetState = transition.to;
						m_lastResolvedTargetClipIndex = target ? target->clipIndex : -1;
						m_lastResolvedTargetFound = false;
						m_lastTransitionDebug = "Transition passed but target state/clip was invalid for " + transition.from + " -> " + transition.to;
					}
					break;
				}
			}
		}
		m_animator->Update(dt);
	}
}

Animator* AnimatorComponent::GetAnimator()
{
	return m_animator.get();
}

const Animator* AnimatorComponent::GetAnimator() const
{
	return m_animator.get();
}

const std::vector<AnimatorComponent::State>& AnimatorComponent::States() const
{
	return m_states;
}

const std::vector<AnimatorComponent::Transition>& AnimatorComponent::Transitions() const
{
	return m_transitions;
}

const std::vector<AnimatorComponent::Condition>& AnimatorComponent::Conditions(const Transition& transition) const
{
	return transition.conditions;
}

const std::string& AnimatorComponent::InitialState() const
{
	return m_initialState;
}

const std::string& AnimatorComponent::CurrentState() const
{
	return m_currentState;
}

const std::string& AnimatorComponent::DesiredState() const
{
	return m_desiredState;
}

std::string AnimatorComponent::LastTransitionDebug() const
{
	return m_lastTransitionDebug;
}

const std::string& AnimatorComponent::LastTransitionFrom() const
{
	return m_lastTransitionFrom;
}

const std::string& AnimatorComponent::LastTransitionTo() const
{
	return m_lastTransitionTo;
}

const std::string& AnimatorComponent::LastTransitionLeftOperandText() const
{
	return m_lastTransitionLeftOperandText;
}

const std::string& AnimatorComponent::LastTransitionRightOperandText() const
{
	return m_lastTransitionRightOperandText;
}

std::string AnimatorComponent::LastTransitionComparatorText() const
{
	return ComparatorToString(m_lastTransitionComparator);
}

float AnimatorComponent::LastTransitionLeftValue() const
{
	return m_lastTransitionLeftValue;
}

float AnimatorComponent::LastTransitionRightValue() const
{
	return m_lastTransitionRightValue;
}

bool AnimatorComponent::LastTransitionPassed() const
{
	return m_lastTransitionPassed;
}

const std::string& AnimatorComponent::LastResolvedTargetState() const
{
	return m_lastResolvedTargetState;
}

int AnimatorComponent::LastResolvedTargetClipIndex() const
{
	return m_lastResolvedTargetClipIndex;
}

bool AnimatorComponent::LastResolvedTargetFound() const
{
	return m_lastResolvedTargetFound;
}

void AnimatorComponent::SetDesiredState(const std::string& stateName)
{
	if (!stateName.empty() && FindState(stateName))
	{
		m_desiredState = stateName;
	}
}

void AnimatorComponent::SetInitialState(const std::string& stateName)
{
	const State* state = FindState(stateName);
	if (!state || !m_animator)
	{
		return;
	}

	m_initialState = state->name;
	m_currentState = state->name;
	m_desiredState = state->name;
	m_transitionCooldown = 0.0f;
	m_animator->Play(state->clipIndex, 0.0f);
}

bool AnimatorComponent::AddState(std::string name, int clipIndex)
{
	if (!m_animator || name.empty() || clipIndex < 0 || clipIndex >= m_animator->ClipCount())
	{
		return false;
	}

	if (FindState(name))
	{
		return false;
	}

	m_states.push_back({ std::move(name), clipIndex });
	if (m_currentState.empty())
	{
		m_initialState = m_states.back().name;
	}
	return true;
}

bool AnimatorComponent::AddTransition(std::string from, std::string to, float blendSeconds, Condition condition)
{
	if (!FindState(from) || !FindState(to) || blendSeconds < 0.0f)
	{
		return false;
	}

	std::vector<Condition> conditions;
	conditions.push_back(condition);
	return AddTransition(std::move(from), std::move(to), blendSeconds, std::move(conditions));
}

bool AnimatorComponent::AddTransition(std::string from, std::string to, float blendSeconds, std::vector<Condition> conditions)
{
	if (!FindState(from) || !FindState(to) || blendSeconds < 0.0f || conditions.empty())
	{
		return false;
	}

	m_transitions.push_back({ std::move(from), std::move(to), blendSeconds, conditions.front(), std::move(conditions) });
	return true;
}

bool AnimatorComponent::UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, Condition condition)
{
	std::vector<Condition> conditions;
	conditions.push_back(condition);
	return UpdateTransition(index, std::move(from), std::move(to), blendSeconds, std::move(conditions));
}

bool AnimatorComponent::UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, std::vector<Condition> conditions)
{
	if (index >= m_transitions.size() || !FindState(from) || !FindState(to) || blendSeconds < 0.0f || conditions.empty())
	{
		return false;
	}

	m_transitions[index] = { std::move(from), std::move(to), blendSeconds, conditions.front(), std::move(conditions) };
	return true;
}

bool AnimatorComponent::RemoveTransition(std::size_t index)
{
	if (index >= m_transitions.size())
	{
		return false;
	}

	m_transitions.erase(m_transitions.begin() + static_cast<std::ptrdiff_t>(index));
	return true;
}

void AnimatorComponent::ActivateState(const std::string& stateName)
{
	const State* state = FindState(stateName);
	if (!state || !m_animator)
	{
		return;
	}

	m_currentState = state->name;
	m_desiredState = state->name;
}

bool AnimatorComponent::TransitionConditionPasses(
	const Transition& transition,
	const Entity& owner,
	float& leftValue,
	float& rightValue,
	bool& operandsResolved) const
{
	const bool leftResolved = ResolveOperand(transition.condition.left, owner, leftValue);
	const bool rightResolved = ResolveOperand(transition.condition.right, owner, rightValue);
	operandsResolved = leftResolved && rightResolved;
	return operandsResolved && Compare(leftValue, rightValue, transition.condition.comparator);
}

bool AnimatorComponent::ResolveOperand(const Operand& operand, const Entity& owner, float& value) const
{
	if (operand.type == OperandType::Constant)
	{
		value = operand.constantValue;
		return true;
	}

	if (operand.memberName.empty())
	{
		return false;
	}

	if (operand.componentName.empty())
	{
		return owner.TryGetBindableValue(operand.memberName, value);
	}

	const Component* component = owner.GetComponentByName(operand.componentName);
	if (!component && operand.componentName == "Controller")
	{
		// Preserve bindings saved before Controller was split into base and player components.
		component = owner.GetComponent<Controller>();
	}
	return component && component->TryGetBindableValue(operand.memberName, value);
}

bool AnimatorComponent::Compare(float lhs, float rhs, Comparator comparator) const
{
	switch (comparator)
	{
	case Comparator::NotEqual: return lhs != rhs;
	case Comparator::Greater: return lhs > rhs;
	case Comparator::Less: return lhs < rhs;
	case Comparator::GreaterEqual: return lhs >= rhs;
	case Comparator::LessEqual: return lhs <= rhs;
	case Comparator::Equal:
	default:
		return lhs == rhs;
	}
}

const AnimatorComponent::Transition* AnimatorComponent::FindTransition(const std::string& from, const std::string& to) const
{
	for (const auto& transition : m_transitions)
	{
		if ((transition.from == from || transition.from == "*") && transition.to == to)
		{
			return &transition;
		}
	}
	return nullptr;
}

const AnimatorComponent::State* AnimatorComponent::FindState(const std::string& name) const
{
	for (const auto& state : m_states)
	{
		if (state.name == name)
		{
			return &state;
		}
	}
	return nullptr;
}

