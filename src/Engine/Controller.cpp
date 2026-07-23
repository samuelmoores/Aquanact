#include "Engine/Controller.h"

#include "Engine/Input.h"
#include "Engine/Globals.h"
#include "Engine/Debug.h"
#include "Engine/RenderManager.h"
#include "Engine/Entity.h"

#include <cmath>

void Controller::Update(Entity&, float dt)
{
	if (m_owner == nullptr)
	{
		gDebug.SetGameplayDiagnostics(m_registered, "<unbound>", glm::vec3(0.0f), m_moveSpeed, dt, glm::vec3(0.0f), glm::vec3(0.0f));
		return;
	}

	const glm::vec3 moveInput = gInput.MoveInput();
	if (moveInput.x == 0.0f && moveInput.z == 0.0f)
	{
		gDebug.SetGameplayDiagnostics(m_registered, m_owner->Name(), moveInput, m_moveSpeed, dt, glm::vec3(0.0f), m_owner->Position());
		return;
	}

	glm::vec3 forward = gRenderManager.GetGameCamera().GetFacing();
	forward.y = 0.0f;
	if (glm::length(forward) <= 0.0001f)
	{
		forward = glm::vec3(0.0f, 0.0f, 1.0f);
	}
	forward = glm::normalize(forward);

	glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
	if (glm::length(right) <= 0.0001f)
	{
		right = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	right = glm::normalize(right);

	glm::vec3 movement = forward * moveInput.z + right * moveInput.x;
	movement.y = 0.0f;
	if (glm::length(movement) <= 0.0001f)
	{
		gDebug.SetGameplayDiagnostics(m_registered, m_owner->Name(), moveInput, m_moveSpeed, dt, glm::vec3(0.0f), m_owner->Position());
		return;
	}

	movement = glm::normalize(movement);
	const float targetYaw = std::atan2(movement.x, movement.z);
	m_owner->SetRotation(glm::vec3(m_owner->Rotation().x, targetYaw, m_owner->Rotation().z));

	const glm::vec3 delta = movement * m_moveSpeed * dt;
	m_owner->Move(delta);
	gDebug.SetGameplayDiagnostics(m_registered, m_owner->Name(), moveInput, m_moveSpeed, dt, delta, m_owner->Position());
}

