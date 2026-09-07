#pragma once

#include "Engine/Core/Component.h"
#include "Engine/Core/Animator.h"

#include <string>
#include <vector>

class Mesh;

// Animation-only component for cutscene entities. Unlike EntityStateMachine,
// this has no gameplay states, transitions, input, or movement behavior.
class CutsceneAnimator final : public Component
{
public:
	explicit CutsceneAnimator(Mesh* mesh);

	const char* Name() const override { return "CutsceneAnimator"; }
	Animator* GetAnimator() { return m_animator.get(); }
	const Animator* GetAnimator() const { return m_animator.get(); }
	const std::vector<std::string>& AnimationNames() const { return m_animationNames; }
	int FindAnimationIndex(const std::string& animationName) const;

private:
	std::unique_ptr<Animator> m_animator;
	std::vector<std::string> m_animationNames;
};
