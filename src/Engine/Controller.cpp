#include "Engine/Controller.h"

#include "Engine/Input.h"
#include "Engine/Globals.h"
#include "Engine/Debug.h"
#include "Engine/RenderManager.h"
#include "Engine/Entity.h"

#include <cmath>

std::vector<BindableMember> Controller::GetBindableMembers() const
{
	return {
		{ "IsMoving", "Is Moving", "bool", BindableMember::Kind::Function },
		{ "MoveSpeed", "Move Speed", "float", BindableMember::Kind::Function },
	};
}

bool Controller::TryGetBindableValue(const std::string& memberName, float& value) const
{
	if (memberName == "IsMoving")
	{
		value = m_isMoving ? 1.0f : 0.0f;
		return true;
	}
	if (memberName == "MoveSpeed")
	{
		value = m_moveSpeed;
		return true;
	}
	return false;
}

void Controller::Update(Entity&, float dt)
{
	Entity* owner = Owner();
	if (owner == nullptr)
	{
		gDebug.SetGameplayDiagnostics("<unbound>", glm::vec3(0.0f), m_moveSpeed, dt, glm::vec3(0.0f), glm::vec3(0.0f));
		return;
	}

	const glm::vec3 moveInput = gInput.MoveInput();
	const float inputMagnitude = glm::length(glm::vec2(moveInput.x, moveInput.z));
	if (inputMagnitude <= m_movementDeadzone)
	{
		m_isMoving = false;
		gDebug.SetGameplayDiagnostics(owner->Name(), moveInput, m_moveSpeed, dt, glm::vec3(0.0f), owner->Position());
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
		m_isMoving = false;
		gDebug.SetGameplayDiagnostics(owner->Name(), moveInput, m_moveSpeed, dt, glm::vec3(0.0f), owner->Position());
		return;
	}

	movement = glm::normalize(movement);
	m_isMoving = true;
	const float targetYaw = std::atan2(movement.x, movement.z);
	owner->SetRotation(glm::vec3(owner->Rotation().x, targetYaw, owner->Rotation().z));

	const glm::vec3 delta = movement * m_moveSpeed * dt;
	owner->Move(delta);
	gDebug.SetGameplayDiagnostics(owner->Name(), moveInput, m_moveSpeed, dt, delta, owner->Position());
}

