#include "Animation.h"

Animation::Animation(aiAnimation* anim, Mesh* mesh)
{
    m_assimpAnim = anim;
	m_blendFactor = 0.0f;
}

float Animation::duration()
{
    return m_duration;
}

float Animation::ticksPerSecond()
{
    return m_ticksPerSecond;
}

void Animation::Run(float deltaTime)
{
	m_blendFactor += deltaTime;
    /*
    hack, when new anim starts, blend factor is set to 0
	ideally, if skinned -> RunAnim(), deal with blending inside anim class
	if (m_blendFactor < 1.0f)
	{
		objects[i]->GetMesh()->BlendAnimation(nextAnim, Engine::TimeElapsed(), objects[i]->BlendFactor());

		if (m_blendFactor >= 1.0f)
		{
			objects[i]->GetMesh()->SetCurrentAnim(nextAnim);
		}
	}
	else
	{
		objects[i]->GetMesh()->RunAnimation(Engine::TimeElapsed());
	}
    */
}


