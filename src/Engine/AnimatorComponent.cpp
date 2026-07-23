#include "Engine/AnimatorComponent.h"

#include "Engine/Entity.h"
#include "Engine/Animation.h"
#include <algorithm>
#include <filesystem>

namespace {
	std::string MakeStateNameFromSource(const std::string& sourcePath, int clipIndex)
	{
		std::filesystem::path p(sourcePath);
		std::string name = p.stem().string();
		if (name.empty())
		{
			name = "Clip";
		}
		return name + " #" + std::to_string(clipIndex);
	}
}

AnimatorComponent::AnimatorComponent(Mesh* mesh)
{
	if (!mesh)
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
			m_animator->AddClip(new Animation(mesh->GetAnimation(i)));
			AddState(MakeStateNameFromSource(mesh->GetAnimationSource(i), i), i);
		}
		m_currentState = m_states.front().name;
		m_desiredState = m_currentState;
		m_animator->Play(0, 0.0f);
	}
}

const char* AnimatorComponent::Name() const
{
	return "Animator";
}

void AnimatorComponent::Update(Entity&, float dt)
{
	if (m_animator)
	{
		if (!m_desiredState.empty() && m_desiredState != m_currentState)
		{
			const State* target = FindState(m_desiredState);
			if (target && target->clipIndex >= 0)
			{
				m_animator->Play(target->clipIndex, 0.25f);
				ActivateState(m_desiredState);
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

const std::string& AnimatorComponent::CurrentState() const
{
	return m_currentState;
}

const std::string& AnimatorComponent::DesiredState() const
{
	return m_desiredState;
}

void AnimatorComponent::SetDesiredState(const std::string& stateName)
{
	if (!stateName.empty() && FindState(stateName))
	{
		m_desiredState = stateName;
	}
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
		m_currentState = m_states.back().name;
		m_desiredState = m_currentState;
		m_animator->Play(clipIndex, 0.0f);
	}
	return true;
}

bool AnimatorComponent::AddTransition(std::string from, std::string to, float blendSeconds)
{
	if (!FindState(from) || !FindState(to) || blendSeconds < 0.0f)
	{
		return false;
	}

	m_transitions.push_back({ std::move(from), std::move(to), blendSeconds });
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

