#pragma once
#include <vector>
#include <glm/glm.hpp>

class Object3D;

class PlayerController {
public:
	PlayerController(std::vector<Object3D*> objects);
	void Update();

private:
	std::vector<Object3D*> m_objects;
	float m_currRot   = 0.0f;
	float m_nextRot   = 0.0f;
	bool  m_blendRot  = false;
	bool  m_wasMoving = false;
};
