#pragma once
#include <vector>
#include <glm/glm.hpp>

class Object3D;

class PlayerController {
public:
	PlayerController(std::vector<Object3D*> objects);
	void Update();
	void DrawCapsule();

	float Health() const { return m_health; }
	int   Score()  const { return m_score; }

	void TakeDamage(float amount);
	void AddScore(int points);

private:
	std::vector<Object3D*> m_objects;
	float m_currRot   = 0.0f;
	float m_nextRot   = 0.0f;
	bool  m_blendRot  = false;
	bool  m_wasMoving = false;

	float m_health    = 1.0f;   // 0.0 – 1.0
	int   m_score     = 0;

	bool  m_inTrigger = false;
};
