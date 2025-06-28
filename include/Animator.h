#pragma once
#include "Animation.h"
#include "glm/gtc/matrix_transform.hpp"  
#include "glm/gtx/quaternion.hpp"

class Animator {
public:
	Animator(Animation* animation);
	void Update(float dt);
	void CalculateBoneTransform(const aiNode* node, const glm::mat4& parentTransform);
private:
	Animation* m_animation;
	float m_currentTime;
	std::vector<glm::mat4> m_finalBoneMatrices;
	const aiNode* m_root;
};
