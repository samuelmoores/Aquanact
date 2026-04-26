#include "MathUtils.h"
#include <glm/gtc/constants.hpp>

float MathUtils::ShortestAngleDelta(float from, float to)
{
	float delta = to - from;
	while (delta >  glm::pi<float>()) delta -= glm::two_pi<float>();
	while (delta < -glm::pi<float>()) delta += glm::two_pi<float>();
	return delta;
}
