#pragma once  
#include <iostream>
#include "glm/glm.hpp"  

class Animation {
public:
	Animation(const char* file);
	float duration();
	float ticksPerSecond();
private:
	float m_duration;
	float m_ticksPerSecond;
};
