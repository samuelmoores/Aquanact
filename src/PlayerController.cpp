#include "PlayerController.h"
#include "Physics.h"
#include "MathUtils.h"
#include "Engine.h"

PlayerController::PlayerController(std::vector<Object3D*> objects)
	: m_objects(std::move(objects)) {}

void PlayerController::Update()
{
	glm::vec2 inputAxes = Engine::Input->MoveInput();
	glm::vec3 forward   = Engine::Camera->Forward();
	glm::vec3 right     = Engine::Camera->Right();
	glm::vec3 moveDir   = forward * inputAxes.x + right * inputAxes.y;
	bool isMoving = glm::length(moveDir) > 0.01f;

	// Animation transitions
	if (!m_wasMoving && isMoving)
	{
		m_objects[0]->GetMesh()->SetNextAnim(1);
		m_objects[0]->StartAnimBlend();
	}
	if (m_wasMoving && !isMoving)
	{
		m_objects[0]->GetMesh()->SetNextAnim(0);
		m_objects[0]->StartAnimBlend();
	}
	m_wasMoving = isMoving;

	// Movement + collision
	const float moveSpeed = 500.0f;
	glm::vec3 movement(0.0f);
	if (isMoving)
		movement = glm::normalize(moveDir) * moveSpeed * Engine::DeltaFrameTime();

	Mesh* playerMesh = m_objects[0]->GetMesh();

	bool collided = false;
	for (int i = 1; i < (int)m_objects.size() - 1; i++)
	{
		Mesh* wallMesh = m_objects[i]->GetMesh();
		if (Physics::SweepAABB(playerMesh->minBounds(), playerMesh->maxBounds(), movement, wallMesh->minBounds(), wallMesh->maxBounds()))
		{
			collided = true;
			break;
		}
	}

	if (!collided)
		m_objects[0]->Move(movement);

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
