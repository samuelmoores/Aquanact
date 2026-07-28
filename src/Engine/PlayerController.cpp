#include "Engine/PlayerController.h"

#include "Engine/Entity.h"
#include "Engine/Globals.h"
#include "Engine/Input.h"
#include "Engine/RenderManager.h"

void PlayerController::startUp(Entity&)
{
	if (!m_inputDevice)
	{
		m_inputDevice = &gInput;
	}
}

void PlayerController::Update(Entity& owner, float dt)
{
	const Input* inputDevice = m_inputDevice ? m_inputDevice : &gInput;
	const glm::vec3 moveInput = inputDevice->MoveInput();
	SetDiagnosticInput(moveInput);

	if (glm::length(glm::vec2(moveInput.x, moveInput.z)) <= m_movementDeadzone)
	{
		StopMoving();
		SetDiagnosticInput(moveInput);
		Controller::Update(owner, dt);
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
	SetMovementDirection(movement);
	SetDiagnosticInput(moveInput);
	Controller::Update(owner, dt);
}
