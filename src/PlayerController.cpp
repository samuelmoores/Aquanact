#include "PlayerController.h"
#include "Engine.h"

static bool AABBIntersect(const glm::vec3& minA, const glm::vec3& maxA, const glm::vec3& minB, const glm::vec3& maxB)
{
	return (minA.x <= maxB.x && maxA.x >= minB.x)
		&& (minA.y <= maxB.y && maxA.y >= minB.y)
		&& (minA.z <= maxB.z && maxA.z >= minB.z);
}

static float ShortestAngleDelta(float from, float to)
{
	float delta = to - from;
	while (delta >  glm::pi<float>()) delta -= glm::two_pi<float>();
	while (delta < -glm::pi<float>()) delta += glm::two_pi<float>();
	return delta;
}

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
	const float moveSpeed = 250.0f;
	glm::vec3 movement(0.0f);
	if (isMoving)
		movement = glm::normalize(moveDir) * moveSpeed * Engine::DeltaFrameTime();

	Mesh* playerMesh = m_objects[0]->GetMesh();
	glm::vec3 nextMin = playerMesh->minBounds() + movement;
	glm::vec3 nextMax = playerMesh->maxBounds() + movement;

	bool collided = false;
	for (int i = 1; i < (int)m_objects.size() - 1; i++)
	{
		Mesh* wallMesh = m_objects[i]->GetMesh();
		if (AABBIntersect(nextMin, nextMax, wallMesh->minBounds(), wallMesh->maxBounds()))
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
		float delta = ShortestAngleDelta(m_currRot, m_nextRot);
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
