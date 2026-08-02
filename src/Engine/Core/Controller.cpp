#include "Engine/Core/Controller.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Physics.h"
#include "Engine/Core/SceneManager.h"

#include <cmath>

namespace
{
	void BuildVerticalCapsule(const glm::vec3& boxMin, const glm::vec3& boxMax,
		glm::vec3& base, glm::vec3& tip, float& radius)
	{
		const glm::vec3 center = (boxMin + boxMax) * 0.5f;
		const glm::vec3 halfExtents = (boxMax - boxMin) * 0.5f;
		radius = glm::min(halfExtents.x, halfExtents.z);
		radius = glm::min(radius, halfExtents.y);
		radius = glm::max(radius, 0.001f);

		const float baseY = boxMin.y + radius;
		const float tipY = boxMax.y - radius;
		base = glm::vec3(center.x, glm::min(baseY, tipY), center.z);
		tip = glm::vec3(center.x, glm::max(baseY, tipY), center.z);
	}
}

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

glm::vec3 Controller::MoveWithCollision(Entity& owner, const glm::vec3& delta)
{
	if (glm::length(delta) <= 0.0001f)
	{
		return glm::vec3(0.0f);
	}

	Scene* activeLevel = Root::Current().Levels().ActiveLevel();
	glm::vec3 startMin;
	glm::vec3 startMax;
	if (!activeLevel || !owner.WorldAABB(startMin, startMax))
	{
		owner.Move(delta);
		return delta;
	}

	glm::vec3 resolvedDelta(0.0f);
	glm::vec3 remainingMovement = delta;
	const bool useCapsule = owner.GetPhysicsColliderShape() == PhysicsColliderShape::Capsule;
	glm::vec3 capsuleBase;
	glm::vec3 capsuleTip;
	float capsuleRadius = 0.0f;
	if (useCapsule)
	{
		BuildVerticalCapsule(startMin, startMax, capsuleBase, capsuleTip, capsuleRadius);
	}
	constexpr int maxSlideIterations = 4;
	constexpr float collisionSkin = 0.001f;
	for (int iteration = 0; iteration < maxSlideIterations && glm::length(remainingMovement) > 0.0001f; ++iteration)
	{
		const glm::vec3 currentMin = startMin + resolvedDelta;
		const glm::vec3 currentMax = startMax + resolvedDelta;
		Physics::SweepCollision earliestHit;

		for (const auto& object : activeLevel->Objects())
		{
			if (!object || object.get() == &owner || !object->GetMesh())
			{
				continue;
			}

			glm::vec3 boxMin;
			glm::vec3 boxMax;
			if (!object->WorldAABB(boxMin, boxMax))
			{
				continue;
			}

			Physics::SweepCollision hit;
			if (useCapsule)
			{
				hit = Physics::GetCapsuleAABBSweep(
					capsuleBase + resolvedDelta,
					capsuleTip + resolvedDelta,
					capsuleRadius, remainingMovement, boxMin, boxMax);
			}
			else
			{
				hit = Physics::GetAABBSweep(
					currentMin, currentMax, remainingMovement, boxMin, boxMax);
			}
			if (hit.hit && hit.time < earliestHit.time)
			{
				earliestHit = hit;
			}
		}

		if (!earliestHit.hit)
		{
			resolvedDelta += remainingMovement;
			break;
		}

		const float movementLength = glm::length(remainingMovement);
		const float safeTime = glm::max(0.0f, earliestHit.time - collisionSkin / movementLength);
		resolvedDelta += remainingMovement * safeTime;

		glm::vec3 slideMovement = remainingMovement * (1.0f - earliestHit.time);
		const float intoSurface = glm::dot(slideMovement, earliestHit.normal);
		if (intoSurface < 0.0f)
		{
			slideMovement -= earliestHit.normal * intoSurface;
		}
		remainingMovement = slideMovement;
	}

	owner.Move(resolvedDelta);
	return resolvedDelta;
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
		Root::Current().Debugger().SetGameplayDiagnostics(owner.Name(), m_diagnosticInput, m_moveSpeed, dt, glm::vec3(0.0f), owner.Position());
		return;
	}

	const glm::vec3 movement = glm::normalize(m_movementDirection);
	m_isMoving = true;
	const float targetYaw = std::atan2(movement.x, movement.z);
	owner.SetRotation(glm::vec3(owner.Rotation().x, targetYaw, owner.Rotation().z));

	const glm::vec3 delta = movement * m_moveSpeed * dt;
	const glm::vec3 appliedDelta = MoveWithCollision(owner, delta);
	Root::Current().Debugger().SetGameplayDiagnostics(owner.Name(), m_diagnosticInput, m_moveSpeed, dt, appliedDelta, owner.Position());
}
