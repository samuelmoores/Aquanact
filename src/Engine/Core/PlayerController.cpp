#include "Engine/Core/PlayerController.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/RenderManager.h"

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

void PlayerController::startUp(Entity& owner)
{
	Controller::startUp(owner);
}

void PlayerController::Update(Entity& owner, float dt)
{
	const InputManager& input = Root::Current().InputActions();
	const glm::vec2 move2D = input.VectorValue("Move");
	const glm::vec3 moveInput(move2D.x, 0.0f, move2D.y);
	SetDiagnosticInput(moveInput);

	if (glm::length(move2D) <= m_movementDeadzone)
	{
		StopMoving();
		SetDiagnosticInput(moveInput);
		Controller::Update(owner, dt);
		return;
	}

	glm::vec3 forward = Root::Current().Render().GetGameCamera().GetFacing();
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

	glm::vec3 movement = forward * move2D.y + right * move2D.x;
	movement.y = 0.0f;
	SetMovementDirection(movement);
	SetDiagnosticInput(moveInput);

	if (glm::length(movement) <= m_movementDeadzone)
	{
		m_isMoving = false;
		const glm::vec3 appliedDelta = MoveWithPhysics(owner, glm::vec3(0.0f), dt);
		Root::Current().Debugger().SetGameplayDiagnostics(owner.Name(), moveInput, m_moveSpeed, dt, appliedDelta, owner.Position());
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

	const glm::vec3 desiredHorizontalVelocity = normalizedMovement * m_moveSpeed;
	const glm::vec3 appliedDelta = MoveWithPhysics(owner, desiredHorizontalVelocity, dt);
	Root::Current().Debugger().SetGameplayDiagnostics(owner.Name(), moveInput, m_moveSpeed, dt, appliedDelta, owner.Position());
}
