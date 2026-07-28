#include "Engine/Controller.h"

#include "Engine/Debug.h"
#include "Engine/Entity.h"
#include "Engine/Globals.h"

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

void Controller::SetMovementDirection(const glm::vec3& direction)
{
	m_movementDirection = direction;
	m_diagnosticInput = direction;
}

void Controller::StopMoving()
{
	m_movementDirection = glm::vec3(0.0f);
	m_diagnosticInput = glm::vec3(0.0f);
}

void Controller::Update(Entity& owner, float dt)
{
	ApplyMovement(owner, dt);
}

void Controller::ApplyMovement(Entity& owner, float dt)
{
	if (glm::length(m_movementDirection) <= m_movementDeadzone)
	{
		m_isMoving = false;
		gDebug.SetGameplayDiagnostics(owner.Name(), m_diagnosticInput, m_moveSpeed, dt, glm::vec3(0.0f), owner.Position());
		return;
	}

	const glm::vec3 movement = glm::normalize(m_movementDirection);
	m_isMoving = true;
	const float targetYaw = std::atan2(movement.x, movement.z);
	owner.SetRotation(glm::vec3(owner.Rotation().x, targetYaw, owner.Rotation().z));

	const glm::vec3 delta = movement * m_moveSpeed * dt;
	owner.Move(delta);
	gDebug.SetGameplayDiagnostics(owner.Name(), m_diagnosticInput, m_moveSpeed, dt, delta, owner.Position());
}
