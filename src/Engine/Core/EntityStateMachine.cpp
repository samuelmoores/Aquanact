#include "Engine/Core/EntityStateMachine.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Animation.h"
#include "Engine/Core/Controller.h"

#include <algorithm>
#include <sstream>

EntityStateMachine::EntityStateMachine(Mesh* mesh)
{
	if (!mesh || !mesh->Skinned()) {
		return;
	}
	m_animator = std::make_unique<Animator>(mesh->GetRootNode(), mesh->GetSkeletonPtr());
	for (int i = 0; i < mesh->NumAnimations(); ++i) {
		m_animator->AddClip(new Animation(mesh->GetAnimation(i)));
		m_animationNames.push_back(mesh->GetAnimationSource(i));
	}
}

const char* EntityStateMachine::Name() const { return "EntityStateMachine"; }
void EntityStateMachine::startUp(Entity&) {}
void EntityStateMachine::FirstFrame(Entity&)
{
	if (!m_animator) return;
	StartInitialState();
}

void EntityStateMachine::Update(Entity& entity, float dt)
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
	m_currentStateElapsed += dt;
	if (m_transitionCooldown > 0.0f) {
		m_transitionCooldown -= dt;
		if (m_transitionCooldown < 0.0f) m_transitionCooldown = 0.0f;
	}
	if (!m_animator) return;
	if (!m_desiredState.empty() && m_desiredState != m_currentState) {
		if (!CurrentStateCanTransitionOut()) {
			m_lastTransitionDebug = "Held " + m_currentState + " until its clip completed before honoring desired state " + m_desiredState;
		} else {
				const State* target = FindState(m_desiredState);
				const int targetClipIndex = target ? ResolveAnimationClipIndex(*target) : -1;
				if (target && targetClipIndex >= 0) {
					float blendSeconds = 0.25f;
					if (const Transition* transition = FindTransition(m_currentState, m_desiredState)) blendSeconds = transition->blendSeconds;
					m_lastTransitionDebug = "Forced state change to " + m_desiredState + " using blend " + std::to_string(blendSeconds);
					m_animator->Play(targetClipIndex, blendSeconds);
					ActivateState(m_desiredState);
				}
			}
	} else {
		if (m_transitionCooldown <= 0.0f) {
			for (const auto& transition : m_transitions) {
				if (transition.from != m_currentState) continue;
				if (!CurrentStateCanTransitionOut()) break;
				float leftValue = 0.0f, rightValue = 0.0f;
				bool operandsResolved = true, passed = true;
				const std::vector<Condition>& conditions = transition.conditions.empty() ? std::vector<Condition>{ transition.condition } : transition.conditions;
				for (const Condition& condition : conditions) {
					float conditionLeftValue = 0.0f, conditionRightValue = 0.0f;
					const bool leftResolved = ResolveOperand(condition.left, entity, conditionLeftValue);
					const bool rightResolved = ResolveOperand(condition.right, entity, conditionRightValue);
					const bool conditionResolved = leftResolved && rightResolved;
					operandsResolved = operandsResolved && conditionResolved;
					passed = passed && conditionResolved && Compare(conditionLeftValue, conditionRightValue, condition.comparator);
					if (&condition == &conditions.front()) {
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
				if (!passed) {
					m_lastTransitionDebug = operandsResolved ? "Checked " + transition.from + " -> " + transition.to + " and condition failed: " + m_lastTransitionLeftOperandText + " " + ComparatorToString(transition.condition.comparator) + " " + m_lastTransitionRightOperandText : "Checked " + transition.from + " -> " + transition.to + " but a condition binding could not be resolved";
					continue;
				}
				const State* target = FindState(transition.to);
				const int targetClipIndex = target ? ResolveAnimationClipIndex(*target) : -1;
				if (target && targetClipIndex >= 0) {
					m_lastTransitionDebug = "Transition fired: " + transition.from + " -> " + transition.to + " because " + m_lastTransitionLeftOperandText + " " + ComparatorToString(transition.condition.comparator) + " " + m_lastTransitionRightOperandText;
					m_lastResolvedTargetState = target->name;
					m_lastResolvedTargetClipIndex = targetClipIndex;
					m_lastResolvedTargetFound = true;
					m_animator->Play(targetClipIndex, transition.blendSeconds);
					ActivateState(transition.to);
					m_currentStateLockedUntilComplete = !transition.interrupt;
					m_currentStateElapsed = 0.0f;
					m_transitionCooldown = 0.15f;
				} else {
					m_lastResolvedTargetState = transition.to;
					m_lastResolvedTargetClipIndex = targetClipIndex;
					m_lastResolvedTargetFound = false;
					m_lastTransitionDebug = "Transition passed but target state/clip was invalid for " + transition.from + " -> " + transition.to;
				}
				break;
			}
		}
	}
	m_animator->Update(dt);
}

void EntityStateMachine::SetInitialState(const std::string& stateName) { if (const State* initialState = FindState(stateName)) m_initialState = initialState->name; }
void EntityStateMachine::SetDesiredState(const std::string& stateName) { if (!stateName.empty() && FindState(stateName)) m_desiredState = stateName; }
bool EntityStateMachine::AddState(std::string name, std::string animationName, bool blocksMovement, bool blocksInput) { if (name.empty() || FindState(name)) return false; m_states.push_back({ std::move(name), std::move(animationName), blocksMovement, blocksInput }); return true; }
bool EntityStateMachine::UpdateState(std::size_t index, std::string name, std::string animationName, bool blocksMovement, bool blocksInput) { if (index >= m_states.size() || name.empty()) return false; for (std::size_t i = 0; i < m_states.size(); ++i) { if (i != index && m_states[i].name == name) return false; } m_states[index] = { std::move(name), std::move(animationName), blocksMovement, blocksInput }; return true; }
bool EntityStateMachine::RemoveState(std::size_t index) { if (index >= m_states.size()) return false; const std::string removedName = m_states[index].name; m_states.erase(m_states.begin() + static_cast<std::ptrdiff_t>(index)); if (m_initialState == removedName) m_initialState = m_states.empty() ? std::string{} : m_states.front().name; if (m_currentState == removedName) m_currentState.clear(); if (m_desiredState == removedName) m_desiredState.clear(); m_transitions.erase(std::remove_if(m_transitions.begin(), m_transitions.end(), [&](const Transition& transition) { return transition.from == removedName || transition.to == removedName; }), m_transitions.end()); return true; }
bool EntityStateMachine::AddTransition(std::string from, std::string to, float blendSeconds, bool interrupt, Condition condition) { std::vector<Condition> conditions{ condition }; return AddTransition(std::move(from), std::move(to), blendSeconds, interrupt, std::move(conditions)); }
bool EntityStateMachine::AddTransition(std::string from, std::string to, float blendSeconds, bool interrupt, std::vector<Condition> conditions) { if (!FindState(from) || !FindState(to) || blendSeconds < 0.0f || conditions.empty()) return false; m_transitions.push_back({ std::move(from), std::move(to), blendSeconds, interrupt, conditions.front(), std::move(conditions) }); return true; }
bool EntityStateMachine::UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, bool interrupt, Condition condition) { std::vector<Condition> conditions{ condition }; return UpdateTransition(index, std::move(from), std::move(to), blendSeconds, interrupt, std::move(conditions)); }
bool EntityStateMachine::UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, bool interrupt, std::vector<Condition> conditions) { if (index >= m_transitions.size() || !FindState(from) || !FindState(to) || blendSeconds < 0.0f || conditions.empty()) return false; m_transitions[index] = { std::move(from), std::move(to), blendSeconds, interrupt, conditions.front(), std::move(conditions) }; return true; }
bool EntityStateMachine::RemoveTransition(std::size_t index) { if (index >= m_transitions.size()) return false; m_transitions.erase(m_transitions.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
Animator* EntityStateMachine::GetAnimator() { return m_animator.get(); }
const Animator* EntityStateMachine::GetAnimator() const { return m_animator.get(); }
const std::vector<std::string>& EntityStateMachine::AnimationNames() const { return m_animationNames; }
const std::vector<EntityStateMachine::State>& EntityStateMachine::States() const { return m_states; }
const std::vector<EntityStateMachine::Transition>& EntityStateMachine::Transitions() const { return m_transitions; }
const std::vector<EntityStateMachine::Condition>& EntityStateMachine::Conditions(const Transition& transition) const { return transition.conditions; }
const std::string& EntityStateMachine::InitialState() const { return m_initialState; }
const std::string& EntityStateMachine::CurrentState() const { return m_currentState; }
const std::string& EntityStateMachine::DesiredState() const { return m_desiredState; }
bool EntityStateMachine::CurrentStateLockedUntilComplete() const { return m_currentStateLockedUntilComplete; }
bool EntityStateMachine::CurrentStateBlocksMovement() const { const State* state = FindCurrentState(); return state && state->blocksMovement; }
bool EntityStateMachine::CurrentStateBlocksInput() const { const State* state = FindCurrentState(); return state && state->blocksInput; }
std::string EntityStateMachine::LastTransitionDebug() const { return m_lastTransitionDebug; }
const std::string& EntityStateMachine::LastTransitionFrom() const { return m_lastTransitionFrom; }
const std::string& EntityStateMachine::LastTransitionTo() const { return m_lastTransitionTo; }
const std::string& EntityStateMachine::LastTransitionLeftOperandText() const { return m_lastTransitionLeftOperandText; }
const std::string& EntityStateMachine::LastTransitionRightOperandText() const { return m_lastTransitionRightOperandText; }
std::string EntityStateMachine::LastTransitionComparatorText() const { return ComparatorToString(m_lastTransitionComparator); }
float EntityStateMachine::LastTransitionLeftValue() const { return m_lastTransitionLeftValue; }
float EntityStateMachine::LastTransitionRightValue() const { return m_lastTransitionRightValue; }
bool EntityStateMachine::LastTransitionPassed() const { return m_lastTransitionPassed; }
const std::string& EntityStateMachine::LastResolvedTargetState() const { return m_lastResolvedTargetState; }
int EntityStateMachine::LastResolvedTargetClipIndex() const { return m_lastResolvedTargetClipIndex; }
bool EntityStateMachine::LastResolvedTargetFound() const { return m_lastResolvedTargetFound; }
const char* EntityStateMachine::ComparatorToString(Comparator comparator) { switch (comparator) { case Comparator::NotEqual: return "!="; case Comparator::Greater: return ">"; case Comparator::Less: return "<"; case Comparator::GreaterEqual: return ">="; case Comparator::LessEqual: return "<="; case Comparator::Equal: default: return "=="; } }
std::string EntityStateMachine::OperandToString(const Operand& operand) { if (operand.type == OperandType::Constant) { std::ostringstream value; value << operand.constantValue; return value.str(); } const std::string sourceName = operand.componentName.empty() ? "Entity" : operand.componentName; return sourceName + "." + (operand.memberName.empty() ? "<unbound>" : operand.memberName); }
void EntityStateMachine::StartInitialState()
{
	const State* initialState = FindState(m_initialState);
	if (!initialState) return;
	const int clipIndex = ResolveAnimationClipIndex(*initialState);
	m_currentState = initialState->name;
	m_desiredState = initialState->name;
	m_transitionCooldown = 0.0f;
	m_currentStateElapsed = 0.0f;
	m_currentStateLockedUntilComplete = false;
	if (clipIndex >= 0 && m_animator) m_animator->Play(clipIndex, 0.0f);
}
void EntityStateMachine::ActivateState(const std::string& stateName) { if (const State* state = FindState(stateName); state && m_animator) { m_currentState = state->name; m_desiredState = state->name; m_currentStateElapsed = 0.0f; m_currentStateLockedUntilComplete = false; } }
int EntityStateMachine::ResolveAnimationClipIndex(const State& state) const
{
	if (!m_animator || state.animationName.empty())
	{
		return -1;
	}
	for (std::size_t i = 0; i < m_animationNames.size(); ++i)
	{
		if (m_animationNames[i] == state.animationName)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}
bool EntityStateMachine::TransitionConditionPasses(const Transition& transition, const Entity& entity, float& leftValue, float& rightValue, bool& operandsResolved) const { const bool leftResolved = ResolveOperand(transition.condition.left, entity, leftValue); const bool rightResolved = ResolveOperand(transition.condition.right, entity, rightValue); operandsResolved = leftResolved && rightResolved; return operandsResolved && Compare(leftValue, rightValue, transition.condition.comparator); }
bool EntityStateMachine::ResolveOperand(const Operand& operand, const Entity& entity, float& value) const { if (operand.type == OperandType::Constant) { value = operand.constantValue; return true; } if (operand.memberName.empty()) return false; if (operand.componentName.empty()) return entity.TryGetBindableValue(operand.memberName, value); const Component* component = entity.GetComponentByName(operand.componentName); if (!component && operand.componentName == "Controller") component = entity.GetComponent<Controller>(); return component && component->TryGetBindableValue(operand.memberName, value); }
bool EntityStateMachine::Compare(float lhs, float rhs, Comparator comparator) const { switch (comparator) { case Comparator::NotEqual: return lhs != rhs; case Comparator::Greater: return lhs > rhs; case Comparator::Less: return lhs < rhs; case Comparator::GreaterEqual: return lhs >= rhs; case Comparator::LessEqual: return lhs <= rhs; case Comparator::Equal: default: return lhs == rhs; } }
const EntityStateMachine::Transition* EntityStateMachine::FindTransition(const std::string& from, const std::string& to) const { for (const auto& transition : m_transitions) if ((transition.from == from || transition.from == "*") && transition.to == to) return &transition; return nullptr; }
const EntityStateMachine::State* EntityStateMachine::FindState(const std::string& name) const { for (const auto& state : m_states) if (state.name == name) return &state; return nullptr; }
const EntityStateMachine::State* EntityStateMachine::FindCurrentState() const { return FindState(m_currentState); }
bool EntityStateMachine::CurrentStateCanTransitionOut() const
{
	if (!m_currentStateLockedUntilComplete) return true;
	const State* state = FindCurrentState();
	if (!state || !m_animator) return true;
	const int clipIndex = ResolveAnimationClipIndex(*state);
	const float clipDuration = clipIndex >= 0 ? m_animator->ClipDuration(clipIndex) : 0.0f;
	if (clipDuration <= 0.0f) return true;
	return m_currentStateElapsed >= clipDuration;
}
