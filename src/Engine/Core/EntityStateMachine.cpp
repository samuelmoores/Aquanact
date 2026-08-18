#include "Engine/Core/EntityStateMachine.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Animation.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Audio.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FileSystem.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <random>
#include <sstream>

// -----------------------------------------------------------------------------
// Construction and lifecycle.
// -----------------------------------------------------------------------------

EntityStateMachine::EntityStateMachine(Mesh* mesh)
{
	if (!mesh || !mesh->Skinned())
	{
		return;
	}

	m_animator = std::make_unique<Animator>(mesh->GetRootNode(), mesh->GetSkeletonPtr());
	for (int i = 0; i < mesh->NumAnimations(); ++i)
	{
		m_animator->AddClip(new Animation(mesh->GetAnimation(i)));
		m_animationNames.push_back(mesh->GetAnimationSource(i));
	}
}

const char* EntityStateMachine::Name() const { return "EntityStateMachine"; }
void EntityStateMachine::startUp(Entity&) {}
void EntityStateMachine::FirstFrame(Entity&)
{
	if (!m_animator)
	{
		return;
	}

	StartInitialState();
}

void EntityStateMachine::Update(Entity& entity, float dt)
{
	// Step 1: clear the previous frame's diagnostics and advance state timers.
	ResetTransitionDiagnostics();
	UpdateTimers(dt);

	if (!m_animator)
	{
		return;
	}

	// Step 2: process an explicit desired-state request before normal transitions.
	// A desired state takes priority, so normal transitions are skipped whenever
	// the request is still active, even if its target is not currently valid.
	if (!ProcessDesiredStateChange())
	{
		EvaluateTransitions(entity);
	}

	// Step 3: capture the animation cursor before advancing it so timeline events
	// can be detected across this update, including loop wraparound.
	const int previousClip = m_animator->CurrentClipIndex();
	const float previousTicks = m_animator->CurrentTimeTicks();

	// Step 4: advance the animator after state selection for this frame.
	//***************************************
	//-----------------------------
	//|       animator update     |
	//-----------------------------
	m_animator->Update(dt);
	//-----------------------------
	//|       animator update     |
	//-----------------------------
	//***************************************

	const State* currentState = FindCurrentState();

	// play sound events after animation update
	if (!currentState->soundEvents.empty())
	{
		// get the animation clip index and the current tick
		const int currentClip = m_animator->CurrentClipIndex();
		const float currentTicks = m_animator->CurrentTimeTicks();

		// Only process events when the animator remained on the same clip for the
		// entire update and that clip still belongs to the current state. State
		// transitions can change clips during this frame, in which case comparing
		// the old and new cursors could trigger an event at the wrong time.
		if (currentClip == previousClip && currentClip == ResolveAnimationClipIndex(*currentState))
		{
			PlaySoundEventsCrossed(previousTicks, currentTicks, m_animator->CurrentClipDurationTicks());
		}
	}
}

void EntityStateMachine::PlaySoundEventsCrossed(float previousTicks, float currentTicks, float duration)
{
	if (!Root::Current().State().IsGameMode() || duration <= 0.0f)
	{
		return;
	}

	const State* state = FindCurrentState();
	if (!state)
	{
		return;
	}

	// Check every sound event belonging to the active state. Events are stored
	// as animation-frame positions, while Animator reports its cursor in ticks.
	for (const SoundEvent& event : state->soundEvents)
	{
		// Convert the authored frame index into the corresponding animation tick.
		// The final frame maps to the end of the clip, so frameCount - 1 is used
		// as the divisor. A one-frame clip has no range to convert.
		const int frameCount = m_animator->CurrentClipFrameCount();
		const float eventTicks = frameCount > 1
			? duration * (std::max(0.0f, event.frame - 1.0f) / static_cast<float>(frameCount - 1))
			: event.frame;

		// An event is crossed when the cursor moves past it during this update.
		// If currentTicks is lower than previousTicks, the animation wrapped from
		// the end back to the beginning, so the comparison spans both ranges.
		const bool crossed = previousTicks <= currentTicks
			? (eventTicks > previousTicks && eventTicks <= currentTicks)
			: (eventTicks > previousTicks || eventTicks <= currentTicks);
		if (crossed)
			PlaySoundEvent(event);
	}
}

void EntityStateMachine::PlaySoundEvent(const SoundEvent& event)
{
	const auto resolveAssetPath = [](const std::string& relativePath)
	{
		std::filesystem::path path = std::filesystem::path("assets") / relativePath;
#ifdef AQUANACT_GAME
		path = Root::Current().FileSystemRef().ExecutableDirectory() / path;
#else
#ifdef AQUANACT_SOURCE_ROOT
		if (!std::filesystem::exists(path))
			path = std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / relativePath;
#endif
#endif
		return path;
	};

	if (!event.randomSample)
	{
		if (!Audio::IsSoundLoaded(event.soundName))
		{
			const std::filesystem::path path = resolveAssetPath(event.soundName);
			if (std::filesystem::exists(path))
				Audio::LoadSound(event.soundName, path.string());
		}
		Audio::PlaySound(event.soundName, event.volume);
		return;
	}

	const std::filesystem::path folder = resolveAssetPath(event.soundName);
	std::vector<std::filesystem::path> samples;
	std::error_code error;
	if (!std::filesystem::is_directory(folder, error))
		return;
	for (const auto& entry : std::filesystem::directory_iterator(folder, error))
	{
		if (error || !entry.is_regular_file()) continue;
		const std::string extension = entry.path().extension().string();
		if (extension == ".wav" || extension == ".mp3" || extension == ".ogg" || extension == ".flac")
			samples.push_back(entry.path());
	}
	if (samples.empty()) return;

	static std::mt19937 generator(std::random_device{}());
	const auto selected = samples[std::uniform_int_distribution<std::size_t>(0, samples.size() - 1)(generator)];
	const std::string soundName = std::filesystem::relative(selected, "assets").generic_string();
	if (!Audio::IsSoundLoaded(soundName))
		Audio::LoadSound(soundName, std::filesystem::absolute(selected).string());
	Audio::PlaySound(soundName, event.volume);
}

void EntityStateMachine::PlaySoundEventsAtStateStart()
{
	if (!Root::Current().State().IsGameMode())
	{
		return;
	}

	const State* state = FindCurrentState();
	if (!state)
	{
		return;
	}

	// Frame numbers in the editor are one-based. Trigger every event assigned
	// to frame 1 immediately when the state becomes active, before the first
	// animator update can move the cursor past the start of the clip.
	for (const SoundEvent& event : state->soundEvents)
	{
		if (event.frame > 1.0f)
		{
			continue;
		}

		PlaySoundEvent(event);
		std::cout << "played sound at volume: " << event.volume << std::endl;
	}
}

// -----------------------------------------------------------------------------
// Update phases.
// -----------------------------------------------------------------------------

void EntityStateMachine::ResetTransitionDiagnostics()
{
	// Transition diagnostics represent the last evaluated transition, not only
	// the current frame. Preserve the snapshot while cooldowns or wait gates
	// prevent a new evaluation, so the diagnostics window remains meaningful.
}

void EntityStateMachine::UpdateTimers(float dt)
{
	m_currentStateElapsed += dt;

	if (m_transitionCooldown > 0.0f)
	{
		m_transitionCooldown = std::max(0.0f, m_transitionCooldown - dt);
	}
}

bool EntityStateMachine::ProcessDesiredStateChange()
{
	// A desired state is optional. Returning false tells Update() to continue
	// with ordinary transition evaluation.
	if (m_desiredState.empty() || m_desiredState == m_currentState)
	{
		return false;
	}

	const Transition* desiredTransition = FindTransition(m_currentState, m_desiredState);
	const bool waitingForCurrentState = desiredTransition
		&& desiredTransition->waitForCurrentStateComplete
		&& !CurrentStateCanTransitionOut();

	if (waitingForCurrentState)
	{
		// Keep the requested state pending. The next update will try again after
		// the current clip has completed.
		m_lastTransitionDebug = "Held " + m_currentState
			+ " until its clip completed before honoring desired state " + m_desiredState;
		return true;
	}

	// Resolve the requested state before changing runtime state. A missing state
	// or animation leaves the request active without corrupting playback.
	const State* targetState = FindState(m_desiredState);
	const int targetClipIndex = targetState ? ResolveAnimationClipIndex(*targetState) : -1;
	if (!targetState || targetClipIndex < 0)
	{
		return true;
	}

	// Desired-state changes bypass condition evaluation, but still use the
	// transition's configured blend time when one exists.
	const float blendSeconds = desiredTransition ? desiredTransition->blendSeconds : 0.25f;
	m_lastTransitionDebug = "Forced state change to " + m_desiredState
		+ " using blend " + std::to_string(blendSeconds);
	m_animator->Play(targetClipIndex, blendSeconds);
	ActivateState(m_desiredState);
	return true;
}

void EntityStateMachine::EvaluateTransitions(Entity& owner)
{
	if (m_transitionCooldown > 0.0f)
	{
		return;
	}

	// Transitions are checked in their stored order. Once one fires, the state
	// has changed and the remaining transitions belong to the next update.
	for (const Transition& transition : m_transitions)
	{
		if (transition.from != m_currentState)
		{
			continue;
		}

		if (transition.waitForCurrentStateComplete && !CurrentStateCanTransitionOut())
		{
			m_lastTransitionFrom = transition.from;
			m_lastTransitionTo = transition.to;
			m_lastTransitionWaitBlocked = true;
			m_lastTransitionBlockedReason = "Waiting for current animation to finish before allowing this transition";
			m_lastTransitionDebug = "Blocked " + transition.from + " -> " + transition.to
				+ " because the current state has not finished yet";
			continue;
		}

		float leftValue = 0.0f;
		float rightValue = 0.0f;
		bool operandsResolved = true;
		const bool conditionsPassed = EvaluateTransitionConditions(
			transition,
			owner,
			leftValue,
			rightValue,
			operandsResolved);

		m_lastTransitionFrom = transition.from;
		m_lastTransitionTo = transition.to;
		m_lastTransitionLeftValue = leftValue;
		m_lastTransitionRightValue = rightValue;
		m_lastTransitionPassed = conditionsPassed;
		m_lastTransitionWaitBlocked = false;

		if (!conditionsPassed)
		{
			m_lastTransitionBlockedReason = operandsResolved
				? "Condition failed after evaluating the transition operands"
				: "Condition could not be fully evaluated because one or more bindings were unavailable";
			m_lastTransitionDebug = operandsResolved
				? "Checked " + transition.from + " -> " + transition.to + " and condition failed: "
					+ m_lastTransitionLeftOperandText + " "
					+ ComparatorToString(transition.condition.comparator) + " "
					+ m_lastTransitionRightOperandText
				: "Checked " + transition.from + " -> " + transition.to
					+ " but a condition binding could not be resolved";
			continue;
		}

		if (FireTransition(transition))
		{
			break;
		}
	}
}

bool EntityStateMachine::EvaluateTransitionConditions(
	const Transition& transition,
	const Entity& owner,
	float& leftValue,
	float& rightValue,
	bool& operandsResolved)
{
	// Every condition must resolve both operands and pass its comparison.
	// Resolution is tracked separately so diagnostics can explain the failure.
	bool conditionsPassed = true;
	const std::vector<Condition>& conditions = transition.conditions.empty()
		? std::vector<Condition>{ transition.condition }
		: transition.conditions;

	for (std::size_t conditionIndex = 0; conditionIndex < conditions.size(); ++conditionIndex)
	{
		const Condition& condition = conditions[conditionIndex];

		// Resolve both sides independently before comparing them.
		float conditionLeftValue = 0.0f;
		float conditionRightValue = 0.0f;
		const bool leftResolved = ResolveOperand(condition.left, owner, conditionLeftValue);
		const bool rightResolved = ResolveOperand(condition.right, owner, conditionRightValue);
		const bool conditionResolved = leftResolved && rightResolved;

		operandsResolved = operandsResolved && conditionResolved;
		const bool comparisonPassed = conditionResolved
			&& Compare(conditionLeftValue, conditionRightValue, condition.comparator);
		conditionsPassed = conditionsPassed && comparisonPassed;

		// The UI displays the first condition as the representative condition, while
		// the aggregate result above still includes every condition.
		if (conditionIndex == 0)
		{
			leftValue = conditionLeftValue;
			rightValue = conditionRightValue;
			m_lastTransitionLeftOperandText = OperandToString(condition.left);
			m_lastTransitionRightOperandText = OperandToString(condition.right);
			m_lastTransitionComparator = condition.comparator;
		}
	}

	return conditionsPassed;
}

bool EntityStateMachine::FireTransition(const Transition& transition)
{
	assert(!transition.from.empty());
	assert(!transition.to.empty());

	// Step 1: resolve the destination before changing the current state. This
	// keeps malformed transitions from leaving the machine in a half-state.
	const State* targetState = FindState(transition.to);
	const int targetClipIndex = targetState ? ResolveAnimationClipIndex(*targetState) : -1;
	if (!targetState || targetClipIndex < 0)
	{
		m_lastResolvedTargetState = transition.to;
		m_lastResolvedTargetClipIndex = targetClipIndex;
		m_lastResolvedTargetFound = false;
		m_lastTransitionBlockedReason = "Target state/clip was invalid";
		m_lastTransitionDebug = "Transition passed but target state/clip was invalid for "
			+ transition.from + " -> " + transition.to;
		return false;
	}
	assert(!targetState->name.empty());

	// Step 2: record the successful transition for diagnostics before playback
	// changes the animator's active clip.
	m_lastTransitionDebug = "Transition fired: " + transition.from + " -> " + transition.to + " because "
		+ m_lastTransitionLeftOperandText + " "
		+ ComparatorToString(transition.condition.comparator) + " "
		+ m_lastTransitionRightOperandText;
	m_lastResolvedTargetState = targetState->name;
	m_lastResolvedTargetClipIndex = targetClipIndex;
	m_lastResolvedTargetFound = true;

	// Step 3: play the new clip, activate the state, and reset state-local timing.
	// Playback starts first so animation and state bookkeeping stay synchronized.
	m_animator->Play(targetClipIndex, transition.blendSeconds);
	ActivateState(transition.to);
	m_currentStateElapsed = 0.0f;

	// Step 4: prevent immediate transition thrashing and clear any prior failure.
	m_transitionCooldown = 0.15f;
	m_lastTransitionBlockedReason.clear();
	return true;
}

// -----------------------------------------------------------------------------
// State editing.
// -----------------------------------------------------------------------------

void EntityStateMachine::SetInitialState(const std::string& stateName)
{
	if (const State* state = FindState(stateName))
	{
		m_initialState = state->name;
	}
}

void EntityStateMachine::SetDesiredState(const std::string& stateName)
{
	if (!stateName.empty() && FindState(stateName))
	{
		m_desiredState = stateName;
	}
}

bool EntityStateMachine::AddState(std::string name, std::string animationName, bool blocksMovement, bool blocksInput)
{
	if (name.empty() || FindState(name))
	{
		return false;
	}

	m_states.push_back({ std::move(name), std::move(animationName), blocksMovement, blocksInput });
	return true;
}

bool EntityStateMachine::UpdateState(std::size_t index, std::string name, std::string animationName, bool blocksMovement, bool blocksInput)
{
	if (index >= m_states.size() || name.empty())
	{
		return false;
	}

	for (std::size_t stateIndex = 0; stateIndex < m_states.size(); ++stateIndex)
	{
		if (stateIndex != index && m_states[stateIndex].name == name)
		{
			return false;
		}
	}

	m_states[index] = { std::move(name), std::move(animationName), blocksMovement, blocksInput };
	return true;
}

bool EntityStateMachine::RemoveState(std::size_t index)
{
	if (index >= m_states.size())
	{
		return false;
	}

	const std::string removedName = m_states[index].name;
	m_states.erase(m_states.begin() + static_cast<std::ptrdiff_t>(index));

	// Removing a state also removes any state references that can no longer resolve.
	if (m_initialState == removedName)
	{
		m_initialState = m_states.empty() ? std::string{} : m_states.front().name;
	}
	if (m_currentState == removedName)
	{
		m_currentState.clear();
	}
	if (m_desiredState == removedName)
	{
		m_desiredState.clear();
	}

	m_transitions.erase(
		std::remove_if(m_transitions.begin(), m_transitions.end(), [&](const Transition& transition)
		{
			return transition.from == removedName || transition.to == removedName;
		}),
		m_transitions.end());
	return true;
}

bool EntityStateMachine::AddStateSoundEvent(const std::string& stateName, SoundEvent event)
{
	State* state = nullptr;

	// is there a stateName state?
	for (State& candidate : m_states)
	{
		if (candidate.name == stateName)
		{
			state = &candidate;
			break;
		}
	}

	// is the state valid, does sound exists, is frame non negative
	if (!state || event.soundName.empty() || event.frame < 0.0f || event.volume < 0.0f)
	{
		return false;
	}

	// set volume
	event.volume = std::min(event.volume, 100.0f);

	// add the event
	state->soundEvents.push_back(std::move(event));

	// Keep events in animation order so the runtime can process them from the
	// earliest frame to the latest. A stable sort preserves insertion order when
	// multiple sounds share the same frame, making their playback order predictable.
	std::stable_sort(state->soundEvents.begin(), state->soundEvents.end(), [](const SoundEvent& left, const SoundEvent& right)
	{
		return left.frame < right.frame;
	});

	return true;
}

bool EntityStateMachine::UpdateStateSoundEvent(const std::string& stateName, std::size_t eventIndex, SoundEvent event)
{
	State* state = nullptr;

	for (State& candidate : m_states)
	{
		if (candidate.name == stateName)
		{
			state = &candidate;
			break;
		}
	}

	if (!state || eventIndex >= state->soundEvents.size()
		|| event.soundName.empty() || event.frame < 0.0f || event.volume < 0.0f)
	{
		return false;
	}

	event.volume = std::min(event.volume, 100.0f);
	state->soundEvents[eventIndex] = std::move(event);

	std::stable_sort(state->soundEvents.begin(), state->soundEvents.end(), [](const SoundEvent& left, const SoundEvent& right)
	{
		return left.frame < right.frame;
	});

	return true;
}

bool EntityStateMachine::RemoveStateSoundEvent(const std::string& stateName, std::size_t eventIndex)
{
	State* state = nullptr;
	for (State& candidate : m_states)
	{
		if (candidate.name == stateName)
		{
			state = &candidate;
			break;
		}
	}
	if (!state || eventIndex >= state->soundEvents.size())
	{
		return false;
	}

	state->soundEvents.erase(state->soundEvents.begin() + static_cast<std::ptrdiff_t>(eventIndex));
	return true;
}

// -----------------------------------------------------------------------------
// Transition editing.
// -----------------------------------------------------------------------------

bool EntityStateMachine::AddTransition(std::string from, std::string to, float blendSeconds, bool waitForCurrentStateComplete, Condition condition)
{
	std::vector<Condition> conditions{ condition };
	return AddTransition(std::move(from), std::move(to), blendSeconds, waitForCurrentStateComplete, std::move(conditions));
}

bool EntityStateMachine::AddTransition(std::string from, std::string to, float blendSeconds, bool waitForCurrentStateComplete, std::vector<Condition> conditions)
{
	if (!FindState(from) || !FindState(to) || blendSeconds < 0.0f || conditions.empty())
	{
		return false;
	}

	m_transitions.push_back({ std::move(from), std::move(to), blendSeconds, waitForCurrentStateComplete, conditions.front(), std::move(conditions) });
	return true;
}

bool EntityStateMachine::UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, bool waitForCurrentStateComplete, Condition condition)
{
	std::vector<Condition> conditions{ condition };
	return UpdateTransition(index, std::move(from), std::move(to), blendSeconds, waitForCurrentStateComplete, std::move(conditions));
}

bool EntityStateMachine::UpdateTransition(std::size_t index, std::string from, std::string to, float blendSeconds, bool waitForCurrentStateComplete, std::vector<Condition> conditions)
{
	if (index >= m_transitions.size() || !FindState(from) || !FindState(to) || blendSeconds < 0.0f || conditions.empty())
	{
		return false;
	}

	m_transitions[index] = { std::move(from), std::move(to), blendSeconds, waitForCurrentStateComplete, conditions.front(), std::move(conditions) };
	return true;
}

bool EntityStateMachine::RemoveTransition(std::size_t index)
{
	if (index >= m_transitions.size())
	{
		return false;
	}

	m_transitions.erase(m_transitions.begin() + static_cast<std::ptrdiff_t>(index));
	return true;
}

// -----------------------------------------------------------------------------
// Runtime accessors.
// -----------------------------------------------------------------------------

Animator* EntityStateMachine::GetAnimator()
{
	return m_animator.get();
}

const Animator* EntityStateMachine::GetAnimator() const
{
	return m_animator.get();
}

const std::vector<std::string>& EntityStateMachine::AnimationNames() const
{
	return m_animationNames;
}

const std::vector<EntityStateMachine::State>& EntityStateMachine::States() const
{
	return m_states;
}

const std::vector<EntityStateMachine::Transition>& EntityStateMachine::Transitions() const
{
	return m_transitions;
}

const std::vector<EntityStateMachine::Condition>& EntityStateMachine::Conditions(const Transition& transition) const
{
	return transition.conditions;
}

const std::string& EntityStateMachine::InitialState() const
{
	return m_initialState;
}

const std::string& EntityStateMachine::CurrentState() const
{
	return m_currentState;
}

const std::string& EntityStateMachine::DesiredState() const
{
	return m_desiredState;
}

bool EntityStateMachine::CurrentStateLockedUntilComplete() const
{
	return CurrentStateWaitsForCompletion() && !CurrentStateCanTransitionOut();
}

bool EntityStateMachine::CurrentStateWaitsForCompletion() const
{
	for (const Transition& transition : m_transitions)
	{
		if ((transition.from == m_currentState || transition.from == "*") && transition.waitForCurrentStateComplete)
		{
			return true;
		}
	}
	return false;
}

float EntityStateMachine::CurrentStateElapsedSeconds() const
{
	return m_currentStateElapsed;
}

float EntityStateMachine::CurrentStateClipDurationSeconds() const
{
	if (!m_animator)
	{
		return 0.0f;
	}

	const State* state = FindCurrentState();
	if (!state)
	{
		return 0.0f;
	}

	const int clipIndex = ResolveAnimationClipIndex(*state);
	return clipIndex >= 0 ? m_animator->ClipDuration(clipIndex) : 0.0f;
}

float EntityStateMachine::CurrentStateSecondsUntilUnlock() const
{
	if (!CurrentStateWaitsForCompletion())
	{
		return 0.0f;
	}

	const float clipDuration = CurrentStateClipDurationSeconds();
	if (clipDuration <= 0.0f)
	{
		return 0.0f;
	}

	return std::max(0.0f, clipDuration - m_currentStateElapsed);
}


bool EntityStateMachine::CurrentStateBlocksMovement() const
{
	const State* state = FindCurrentState();
	return state && state->blocksMovement;
}

bool EntityStateMachine::CurrentStateBlocksInput() const
{
	const State* state = FindCurrentState();
	return state && state->blocksInput;
}

// -----------------------------------------------------------------------------
// Debug and inspection data.
// -----------------------------------------------------------------------------

std::string EntityStateMachine::LastTransitionDebug() const
{
	return m_lastTransitionDebug;
}

const std::string& EntityStateMachine::LastTransitionFrom() const
{
	return m_lastTransitionFrom;
}

const std::string& EntityStateMachine::LastTransitionTo() const
{
	return m_lastTransitionTo;
}

const std::string& EntityStateMachine::LastTransitionLeftOperandText() const
{
	return m_lastTransitionLeftOperandText;
}

const std::string& EntityStateMachine::LastTransitionRightOperandText() const
{
	return m_lastTransitionRightOperandText;
}

std::string EntityStateMachine::LastTransitionComparatorText() const
{
	return ComparatorToString(m_lastTransitionComparator);
}

float EntityStateMachine::LastTransitionLeftValue() const
{
	return m_lastTransitionLeftValue;
}

float EntityStateMachine::LastTransitionRightValue() const
{
	return m_lastTransitionRightValue;
}

bool EntityStateMachine::LastTransitionPassed() const
{
	return m_lastTransitionPassed;
}

bool EntityStateMachine::LastTransitionWaitBlocked() const
{
	return m_lastTransitionWaitBlocked;
}

const std::string& EntityStateMachine::LastTransitionBlockedReason() const
{
	return m_lastTransitionBlockedReason;
}

const std::string& EntityStateMachine::LastResolvedTargetState() const
{
	return m_lastResolvedTargetState;
}

int EntityStateMachine::LastResolvedTargetClipIndex() const
{
	return m_lastResolvedTargetClipIndex;
}

bool EntityStateMachine::LastResolvedTargetFound() const
{
	return m_lastResolvedTargetFound;
}

// -----------------------------------------------------------------------------
// String helpers for UI and diagnostics.
// -----------------------------------------------------------------------------

const char* EntityStateMachine::ComparatorToString(Comparator comparator)
{
	switch (comparator)
	{
	case Comparator::NotEqual:
		return "!=";
	case Comparator::Greater:
		return ">";
	case Comparator::Less:
		return "<";
	case Comparator::GreaterEqual:
		return ">=";
	case Comparator::LessEqual:
		return "<=";
	case Comparator::Equal:
	default:
		return "==";
	}
}

std::string EntityStateMachine::OperandToString(const Operand& operand)
{
	if (operand.type == OperandType::Constant)
	{
		std::ostringstream value;
		value << operand.constantValue;
		return value.str();
	}

	const std::string sourceName = operand.componentName.empty()
		? "Entity"
		: operand.componentName;
	const std::string memberName = operand.memberName.empty()
		? "<unbound>"
		: operand.memberName;
	return sourceName + "." + memberName;
}

// -----------------------------------------------------------------------------
// Internal state management.
// -----------------------------------------------------------------------------

void EntityStateMachine::StartInitialState()
{
	const State* initialState = FindState(m_initialState);
	if (!initialState)
	{
		return;
	}

	const int clipIndex = ResolveAnimationClipIndex(*initialState);
	m_currentState = initialState->name;
	m_desiredState = initialState->name;
	m_transitionCooldown = 0.0f;
	m_currentStateElapsed = 0.0f;
	m_currentStateLockedUntilComplete = false;

	if (clipIndex >= 0 && m_animator)
	{
		m_animator->Play(clipIndex, 0.0f);
		PlaySoundEventsAtStateStart();
	}
}

void EntityStateMachine::ActivateState(const std::string& stateName)
{
	const State* state = FindState(stateName);
	if (!state || !m_animator)
	{
		return;
	}

	// Activation resets state-local timing. The transition evaluator owns the
	// cooldown and the wait gate is derived from outgoing transitions.
	m_currentState = state->name;
	m_desiredState = state->name;
	m_currentStateElapsed = 0.0f;
	m_currentStateLockedUntilComplete = false;
	PlaySoundEventsAtStateStart();
}

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

// -----------------------------------------------------------------------------
// Transition condition evaluation and state lookups.
// -----------------------------------------------------------------------------

bool EntityStateMachine::TransitionConditionPasses(
	const Transition& transition,
	const Entity& entity,
	float& leftValue,
	float& rightValue,
	bool& operandsResolved) const
{
	const bool leftResolved = ResolveOperand(transition.condition.left, entity, leftValue);
	const bool rightResolved = ResolveOperand(transition.condition.right, entity, rightValue);
	operandsResolved = leftResolved && rightResolved;
	return operandsResolved && Compare(leftValue, rightValue, transition.condition.comparator);
}

bool EntityStateMachine::ResolveOperand(const Operand& operand, const Entity& entity, float& value) const
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
		return entity.TryGetBindableValue(operand.memberName, value);
	}

	const Component* component = entity.GetComponentByName(operand.componentName);
	if (!component && operand.componentName == "Controller")
	{
		component = entity.GetComponent<Controller>();
	}

	return component && component->TryGetBindableValue(operand.memberName, value);
}

bool EntityStateMachine::Compare(float lhs, float rhs, Comparator comparator) const
{
	switch (comparator)
	{
	case Comparator::NotEqual:
		return lhs != rhs;
	case Comparator::Greater:
		return lhs > rhs;
	case Comparator::Less:
		return lhs < rhs;
	case Comparator::GreaterEqual:
		return lhs >= rhs;
	case Comparator::LessEqual:
		return lhs <= rhs;
	case Comparator::Equal:
	default:
		return lhs == rhs;
	}
}

bool EntityStateMachine::CurrentStateCanTransitionOut() const
{
	const State* state = FindCurrentState();
	if (!state || !m_animator)
	{
		return true;
	}

	const int clipIndex = ResolveAnimationClipIndex(*state);
	const float clipDuration = clipIndex >= 0 ? m_animator->ClipDuration(clipIndex) : 0.0f;
	if (clipDuration <= 0.0f)
	{
		return true;
	}

	return m_currentStateElapsed >= clipDuration;
}

const EntityStateMachine::Transition* EntityStateMachine::FindTransition(
	const std::string& from,
	const std::string& to) const
{
	for (const Transition& transition : m_transitions)
	{
		if ((transition.from == from || transition.from == "*") && transition.to == to)
		{
			return &transition;
		}
	}

	return nullptr;
}

const EntityStateMachine::State* EntityStateMachine::FindState(const std::string& name) const
{
	for (const State& state : m_states)
	{
		if (state.name == name)
		{
			return &state;
		}
	}

	return nullptr;
}

const EntityStateMachine::State* EntityStateMachine::FindCurrentState() const
{
	return FindState(m_currentState);
}
