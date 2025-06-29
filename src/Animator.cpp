#include "Animator.h"

Animator::Animator(Animation* animation) : m_currentTime(0.0f)
{
    m_animation = animation;
}

void Animator::Update(float dt)
{
    m_currentTime += dt * m_animation->ticksPerSecond();
	m_currentTime = fmod(m_currentTime, m_animation->duration());
}

