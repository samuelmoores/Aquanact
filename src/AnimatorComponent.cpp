#include "AnimatorComponent.h"

#include "Object3D.h"
#include "Animation.h"

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
		m_animator->Play(0);
	}
}

const char* AnimatorComponent::Name() const
{
	return "Animator";
}

void AnimatorComponent::Update(Object3D&, float dt)
{
	if (m_animator)
	{
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
