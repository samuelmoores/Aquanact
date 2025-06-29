#pragma once
#include "Animation.h"

class Animator {
public:
	Animator(Animation* animation);
	void Update(float dt);
private:
	Animation* m_animation;
	float m_currentTime;
};
