#include "PlayerController.h"
#include "Physics.h"
#include "MathUtils.h"
#include "Engine.h"
#include "Line.h"
#include <algorithm>
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>

PlayerController::PlayerController(std::vector<Object3D*> objects)
	: m_objects(std::move(objects)) 
{
	m_objects[0]->Move(glm::vec3(0));
}

void PlayerController::Update()
{
	glm::vec2 inputAxes = Engine::Input->MoveInput();
	glm::vec3 forward   = Engine::Camera->Forward();
	glm::vec3 right     = Engine::Camera->Right();
	glm::vec3 moveDir   = forward * inputAxes.x + right * inputAxes.y;
	bool isMoving = glm::length(moveDir) > 0.01f;

	// Animation transitions
	if (!m_wasMoving && isMoving)
		m_objects[0]->GetAnimator()->Play(1);
	if (m_wasMoving && !isMoving)
		m_objects[0]->GetAnimator()->Play(0);
	m_wasMoving = isMoving;

	// Movement + collision
	const float moveSpeed = 500.0f;
	glm::vec3 movement(0.0f);
	if (isMoving)
		movement = glm::normalize(moveDir) * moveSpeed * Engine::DeltaFrameTime();

	

	if (isMoving)
	{
		m_objects[0]->Move(movement);

		for (int iter = 0; iter < 8; iter++)
		{
			bool collidedInThisIter = false;
			for (int i = 1; i < (int)m_objects.size() - 1; i++)
			{
				Mesh* playerMesh = m_objects[0]->GetMesh();
				glm::vec3 pMin = playerMesh->minBounds();
				glm::vec3 pMax = playerMesh->maxBounds();
				float radius = std::max((pMax.x - pMin.x), (pMax.z - pMin.z)) * 0.5f * 0.25f;
				float cx = (pMin.x + pMax.x) * 0.5f;
				float cz = (pMin.z + pMax.z) * 0.5f;
				glm::vec3 capBase(cx, pMin.y + radius, cz);
				glm::vec3 capTip(cx, std::max(pMax.y - radius, capBase.y), cz);

				Mesh* wallMesh = m_objects[i]->GetMesh();
				Physics::Collision col = Physics::GetCapsuleAABBCollision(capBase, capTip, radius, wallMesh->minBounds(), wallMesh->maxBounds(), movement);
				if (col.hit)
				{
					m_objects[0]->Move(col.normal * col.penetration);
					collidedInThisIter = true;
				}
			}
			if (!collidedInThisIter) break;
		}
	}


	// Trigger box – objects[6]
	if ((int)m_objects.size() > 6)
	{
		Mesh* playerMesh = m_objects[0]->GetMesh();
		glm::vec3 pMin = playerMesh->minBounds();
		glm::vec3 pMax = playerMesh->maxBounds();
		Mesh* triggerMesh = m_objects[6]->GetMesh();
		glm::vec3 tCenter = (triggerMesh->minBounds() + triggerMesh->maxBounds()) * 0.5f;
		glm::vec3 tHalf   = (triggerMesh->maxBounds() - triggerMesh->minBounds()) * 1.0f;
		glm::vec3 tMin = tCenter - tHalf;
		glm::vec3 tMax = tCenter + tHalf;
		bool overlapping = pMax.x > tMin.x && pMin.x < tMax.x &&
		                   pMax.y > tMin.y && pMin.y < tMax.y &&
		                   pMax.z > tMin.z && pMin.z < tMax.z;
		if (overlapping && !m_inTrigger)
		{
			m_inTrigger = true;
			Engine::UI->SetImageVisible(true);
		}
		else if (!overlapping && m_inTrigger)
		{
			m_inTrigger = false;
			Engine::UI->SetImageVisible(false);
		}

		if (m_inTrigger && Engine::Input->JustPressed(Action::Interact))
			Engine::UI->SetImageVisible(false);
	}

	// Rotation smoothing
	if (isMoving)
	{
		m_nextRot  = atan2(moveDir.x, moveDir.z);
		m_blendRot = true;
	}

	if (m_blendRot)
	{
		const float speed = 15.0f;
		float delta = MathUtils::ShortestAngleDelta(m_currRot, m_nextRot);
		float t     = 1.0f - exp(-speed * Engine::DeltaFrameTime());
		m_currRot  += delta * t;
		if (fabs(delta) < glm::radians(0.5f))
		{
			m_currRot  = m_nextRot;
			m_blendRot = false;
		}
		m_objects[0]->SetRotation({ 0.0f, m_currRot, 0.0f });
	}
}

void PlayerController::TakeDamage(float amount)
{
    m_health = std::max(0.0f, m_health - amount);
}

void PlayerController::AddScore(int points)
{
    m_score += points;
}

// call in main loop after render loop
void PlayerController::DrawCapsule()
{
	Mesh* playerMesh = m_objects[0]->GetMesh();
	glm::vec3 pMin = playerMesh->minBounds();
	glm::vec3 pMax = playerMesh->maxBounds();
	float radius = std::max((pMax.x - pMin.x), (pMax.z - pMin.z)) * 0.5f * 0.25f;
	float cx = (pMin.x + pMax.x) * 0.5f;
	float cz = (pMin.z + pMax.z) * 0.5f;
	glm::vec3 capBase(cx, pMin.y + radius, cz);
	glm::vec3 capTip(cx, std::max(pMax.y - radius, capBase.y), cz);

	const int N = 16;
	const glm::vec3 col(0.0f, 1.0f, 0.0f);
	std::vector<LineVertex3D> verts;

	auto push = [&](glm::vec3 p) {
		verts.push_back({ p.x, p.y, p.z, col.r, col.g, col.b });
	};

	auto addCircleXZ = [&](glm::vec3 center) {
		for (int i = 0; i < N; i++) {
			float a0 = (float)i       / N * glm::two_pi<float>();
			float a1 = (float)(i + 1) / N * glm::two_pi<float>();
			push(center + glm::vec3(cosf(a0) * radius, 0.0f, sinf(a0) * radius));
			push(center + glm::vec3(cosf(a1) * radius, 0.0f, sinf(a1) * radius));
		}
	};

	// axisA/axisB define the plane, arc goes from 0 to pi (half circle)
	auto addSemiArc = [&](glm::vec3 center, glm::vec3 axisA, glm::vec3 axisB) {
		for (int i = 0; i < N / 2; i++) {
			float a0 = (float)i       / (N / 2) * glm::pi<float>();
			float a1 = (float)(i + 1) / (N / 2) * glm::pi<float>();
			push(center + axisA * (cosf(a0) * radius) + axisB * (sinf(a0) * radius));
			push(center + axisA * (cosf(a1) * radius) + axisB * (sinf(a1) * radius));
		}
	};

	// XZ rings at base and tip sphere centers
	addCircleXZ(capBase);
	addCircleXZ(capTip);

	// 4 vertical lines connecting the rings
	for (int i = 0; i < 4; i++) {
		float a = (float)i / 4.0f * glm::two_pi<float>();
		glm::vec3 offset(cosf(a) * radius, 0.0f, sinf(a) * radius);
		push(capBase + offset);
		push(capTip  + offset);
	}

	// Bottom hemisphere (arcs curve downward)
	addSemiArc(capBase,  glm::vec3(1,0,0), glm::vec3(0,-1,0));
	addSemiArc(capBase,  glm::vec3(0,0,1), glm::vec3(0,-1,0));

	// Top hemisphere (arcs curve upward)
	addSemiArc(capTip,   glm::vec3(1,0,0), glm::vec3(0, 1,0));
	addSemiArc(capTip,   glm::vec3(0,0,1), glm::vec3(0, 1,0));

	Line line(verts);
	line.UpdateProjection(Engine::Camera->GetProjectionMatrix());
	line.draw(Engine::Camera->GetViewMatrix());
}
