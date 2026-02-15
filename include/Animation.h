#pragma once  
#include <iostream>
#include "glm/glm.hpp"  
#include "assimp/anim.h"
#include "Mesh.h"

class Animation {
public:
	Animation(aiAnimation* anim, Mesh* mesh);
	float duration();
	float ticksPerSecond();
	void Run(float deltaTime);
private:
	float m_duration;
	float m_ticksPerSecond;
	aiAnimation* m_assimpAnim;
	float m_blendFactor;
	Mesh* m_mesh;
};
