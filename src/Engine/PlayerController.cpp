#include "Engine/PlayerController.h"

#include "Engine/Entity.h"
#include "Engine/Debug.h"
#include "Engine/Globals.h"
#include "Engine/Input.h"
#include "Engine/RenderManager.h"

#include <algorithm>

float PlayerController::WrapAngle(float angle)
{
	while (angle > glm::pi<float>())
	{
		angle -= glm::two_pi<float>();
	}
	while (angle < -glm::pi<float>())
	{
		angle += glm::two_pi<float>();
	}
	return angle;
}

float PlayerController::ShortestAngleDelta(float from, float to)
{
	return WrapAngle(to - from);
}

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

	if (glm::length(movement) <= m_movementDeadzone)
	{
		m_isMoving = false;
		gDebug.SetGameplayDiagnostics(owner.Name(), moveInput, m_moveSpeed, dt, glm::vec3(0.0f), owner.Position());
		return;
	}

	const glm::vec3 normalizedMovement = glm::normalize(movement);
	m_isMoving = true;

	const float targetYaw = std::atan2(normalizedMovement.x, normalizedMovement.z);
	const float currentYaw = owner.Rotation().y;
	const float yawDelta = ShortestAngleDelta(currentYaw, targetYaw);
	const float maxStep = std::max(0.0f, m_turnSpeed) * dt;
	const float nextYaw = currentYaw + std::clamp(yawDelta, -maxStep, maxStep);
	owner.SetRotation(glm::vec3(owner.Rotation().x, nextYaw, owner.Rotation().z));

	const glm::vec3 delta = normalizedMovement * m_moveSpeed * dt;
	owner.Move(delta);
	gDebug.SetGameplayDiagnostics(owner.Name(), moveInput, m_moveSpeed, dt, delta, owner.Position());
}
