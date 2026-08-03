#include "Engine/Core/Controller.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Physics.h"
#include "Engine/Core/SceneManager.h"

#include <algorithm>
#include <cmath>

namespace
{
	// Imported assets and movement settings use centimeters as world units.
	constexpr float worldUnitsPerMeter = 100.0f;
	constexpr float gravity = -9.81f * worldUnitsPerMeter;
	constexpr float terminalFallSpeed = -55.0f * worldUnitsPerMeter;
	// Calibrate quadratic air drag so a human-sized controller approaches
	// roughly 55 m/s in real-world units.
	constexpr float quadraticDrag = -gravity /
		(terminalFallSpeed * terminalFallSpeed);

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

void Controller::startUp(Entity&)
{
	m_velocity = glm::vec3(0.0f);
	m_grounded = false;
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

glm::vec3 Controller::MoveWithCollision(Entity& owner, const glm::vec3& delta,
	glm::vec3* lastCollisionNormal, bool* collidedWithGround)
{
	if (lastCollisionNormal)
	{
		*lastCollisionNormal = glm::vec3(0.0f);
	}
	if (collidedWithGround)
	{
		*collidedWithGround = false;
	}
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

		if (lastCollisionNormal)
		{
			*lastCollisionNormal = earliestHit.normal;
		}
		if (collidedWithGround && earliestHit.normal.y > 0.5f)
		{
			*collidedWithGround = true;
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

glm::vec3 Controller::MoveWithPhysics(Entity& owner, const glm::vec3& desiredHorizontalVelocity, float dt)
{
	const glm::vec2 currentHorizontal(m_velocity.x, m_velocity.z);
	const glm::vec2 targetHorizontal(desiredHorizontalVelocity.x, desiredHorizontalVelocity.z);
	const glm::vec2 horizontalDifference = targetHorizontal - currentHorizontal;
	glm::vec2 nextHorizontal = currentHorizontal;
	const float horizontalDifferenceLength = glm::length(horizontalDifference);
	if (glm::length(targetHorizontal) > 0.0001f)
	{
		const float acceleration = m_grounded ? m_groundAcceleration : m_airAcceleration;
		const float maxHorizontalChange = std::max(0.0f, acceleration) * dt;
		if (horizontalDifferenceLength <= maxHorizontalChange)
		{
			nextHorizontal = targetHorizontal;
		}
		else if (maxHorizontalChange > 0.0f)
		{
			nextHorizontal = currentHorizontal + horizontalDifference / horizontalDifferenceLength * maxHorizontalChange;
		}
	}
	else if (m_grounded)
	{
		const float maxFrictionChange = std::max(0.0f, m_groundFriction) * dt;
		const float currentSpeed = glm::length(currentHorizontal);
		if (currentSpeed <= maxFrictionChange)
		{
			nextHorizontal = glm::vec2(0.0f);
		}
		else if (maxFrictionChange > 0.0f)
		{
			nextHorizontal = currentHorizontal / currentSpeed * (currentSpeed - maxFrictionChange);
		}
	}
	else
	{
		nextHorizontal *= std::max(0.0f, 1.0f - m_airDrag * dt);
	}
	m_velocity.x = nextHorizontal.x;
	m_velocity.z = nextHorizontal.y;

	m_velocity.y += gravity * dt;
	if (m_velocity.y < 0.0f)
	{
		m_velocity.y -= quadraticDrag * m_velocity.y * std::abs(m_velocity.y) * dt;
	}

	glm::vec3 lastCollisionNormal(0.0f);
	bool collidedWithGround = false;
	const glm::vec3 appliedDelta = MoveWithCollision(owner, m_velocity * dt, &lastCollisionNormal, &collidedWithGround);
	if (glm::dot(lastCollisionNormal, lastCollisionNormal) > 0.0f)
	{
		const float velocityIntoSurface = glm::dot(m_velocity, lastCollisionNormal);
		if (velocityIntoSurface < 0.0f)
		{
			m_velocity -= lastCollisionNormal * velocityIntoSurface;
		}
	}
	if (collidedWithGround)
	{
		m_grounded = true;
		m_velocity.y = 0.0f;
	}
	else
	{
		m_grounded = false;
	}
	return appliedDelta;
}

void Controller::Update(Entity& owner, float dt)
{
	ApplyMovement(owner, dt);
}

void Controller::ApplyMovement(Entity& owner, float dt)
{
	glm::vec3 desiredHorizontalVelocity(0.0f);
	if (glm::length(m_movementDirection) > m_movementDeadzone)
	{
		const glm::vec3 movement = glm::normalize(m_movementDirection);
		m_isMoving = true;
		const float targetYaw = std::atan2(movement.x, movement.z);
		owner.SetRotation(glm::vec3(owner.Rotation().x, targetYaw, owner.Rotation().z));
		desiredHorizontalVelocity = movement * m_moveSpeed;
	}
	else
	{
		m_isMoving = false;
	}

	const glm::vec3 appliedDelta = MoveWithPhysics(owner, desiredHorizontalVelocity, dt);
	Root::Current().Debugger().SetGameplayDiagnostics(owner.Name(), m_diagnosticInput, m_moveSpeed, dt, appliedDelta, owner.Position());
}
