#pragma once
#include <vector>
#include <map>
#include <string>
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
	void SetVoidMessage(int index, std::string msg) { m_voidMessages[index] = std::move(msg); }

private:
	std::vector<Object3D*> m_objects;
	float m_currRot   = 0.0f;
	float m_nextRot   = 0.0f;
	bool  m_blendRot  = false;
	bool  m_wasMoving = false;

	float m_health    = 1.0f;   // 0.0 – 1.0
	int   m_score     = 0;

	bool  m_inTrigger     = false;
	bool  m_inTvTrigger   = false;
	bool  m_inVoidTrigger = false;
	bool  m_keypadActive = false;
	int   m_keypadIndex  = 0; // 0-3 for digits, 4 for EXECUTE
	int   m_keypadDigits[4] = { 0, 0, 0, 0 };

	bool m_upHeld = false;
	bool m_downHeld = false;
	bool m_leftHeld = false;
	bool m_rightHeld = false;
	bool m_enterHeld = false;
	bool m_codeCorrect = false;
	bool m_keypadChecked = false;

	bool  m_movingObject66 = false;
	float m_object66MoveDistanceRemaining = 350.0f;

	std::map<int, std::string> m_voidMessages;
};
