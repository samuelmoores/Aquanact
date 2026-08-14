#include "Engine/Core/Controller.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Physics.h"
#include "Engine/Core/PhysicsWorld.h"

#include <algorithm>
#include <cmath>

namespace
{
	// Imported assets and movement settings use centimeters as world units.
	constexpr float worldUnitsPerMeter = 100.0f;
	constexpr float gravity = -9.81f * worldUnitsPerMeter;
	constexpr float terminalFallSpeed = -55.0f * worldUnitsPerMeter;
	// Keep grounded state through brief contact misses caused by collider seams
	// or uneven ground while the controller is still moving across the surface.
	constexpr float groundedLossThreshold = 0.1f;
	constexpr float walkableGroundNormalY = 0.25f;
	constexpr float groundProbeDistance = 20.0f;

	glm::vec3 ProjectMovementOntoWalkableGround(const glm::vec3& movement,
		const glm::vec3& groundNormal, float maxSlopeAngle)
	{
		const float minimumNormalY = std::cos(glm::radians(maxSlopeAngle));
		if (groundNormal.y <= 0.0f || groundNormal.y < minimumNormalY)
			return movement;

		glm::vec3 projected = movement - groundNormal * glm::dot(movement, groundNormal);
		const float originalSpeed = glm::length(movement);
		if (originalSpeed > 0.0001f && glm::length(projected) > 0.0001f)
			projected = glm::normalize(projected) * originalSpeed;
		return projected;
	}

	void RecordCollisionContact(const Physics::SweepCollision& collision,
		const glm::vec3& movement, glm::vec3* lastCollisionNormal, bool* collidedWithGround)
	{
		// Preserve the latest contact for grounding and velocity response. The
		// world has already selected the earliest hit for this slide iteration.
		if (lastCollisionNormal)
		{
			*lastCollisionNormal = collision.normal;
		}
		if (collidedWithGround && movement.y <= 0.0f &&
			collision.normal.y > walkableGroundNormalY)
		{
			*collidedWithGround = true;
		}
	}

	glm::vec3 ResolveSlideCollision(const Physics::SweepCollision& collision,
		const glm::vec3& remainingMovement, glm::vec3& resolvedDelta, float collisionSkin)
	{
		// Advance to the contact point, leaving the configured skin distance from
		// the surface. The current controller configuration uses zero skin.
		const float movementLength = glm::length(remainingMovement);
		const float skinOffset = movementLength > 0.0f
			? collisionSkin / movementLength
			: 0.0f;
		const float safeTime = glm::max(0.0f, collision.time - skinOffset);
		resolvedDelta += remainingMovement * safeTime;

		// Keep only motion tangent to the contacted surface for the next iteration.
		glm::vec3 slideMovement = remainingMovement * (1.0f - collision.time);
		const float intoSurface = glm::dot(slideMovement, collision.normal);
		if (intoSurface < 0.0f)
		{
			slideMovement -= collision.normal * intoSurface;
		}
		return slideMovement;
	}

}

float Controller::GroundSurfaceAngle() const
{
	return glm::degrees(std::acos(std::clamp(m_groundNormal.y, -1.0f, 1.0f)));
}

void Controller::startUp(Entity&)
{
	m_velocity = glm::vec3(0.0f);
	m_groundedLossTimer = 0.0f;
	m_grounded = false;
	m_isGrounded = false;
}

void Controller::Update(Entity& owner, float dt)
{
	if (dt <= 0.0f)
	{
		return;
	}

	if (m_grounded)
	{
		m_velocity.y = 0.0f;
	}
	else
	{
		m_velocity.y = std::max(m_velocity.y + gravity * GravityScale() * dt, terminalFallSpeed);
	}

	owner.Move(glm::vec3(0.0f, m_velocity.y * dt, 0.0f));
}

void Controller::Move(Entity& owner, const glm::vec2& direction, float dt)
{
	// Keep diagonal input from moving faster than cardinal input.
	const float inputLength = glm::length(direction);

	const glm::vec2 clampedDirection = inputLength > 1.0f ? direction / inputLength : direction;
	const glm::vec3 movementDirection(clampedDirection.x, 0.0f, clampedDirection.y);

	const bool hasMovementInput = glm::length(movementDirection) > 0.0001f;
	m_movementDirection = hasMovementInput ? glm::normalize(movementDirection) : glm::vec3(0.0f);

	m_isMoving = hasMovementInput;

	SetDiagnosticInput(movementDirection);

	if (dt <= 0.0f || !hasMovementInput)
	{
		return;
	}

	const ColliderHandle collider = PhysicsWorld::Instance().Find(owner);
	glm::vec3 minBounds;
	glm::vec3 maxBounds;
	const bool hasValidBounds = owner.WorldAABB(minBounds, maxBounds);

	// Collision sweeping is added in the next movement step. Until then, keep
	// direct movement as the fallback for entities without registered geometry.
	if (collider == InvalidColliderHandle || !hasValidBounds)
	{
		owner.Move(movementDirection * m_moveSpeed * dt);
		return;
	}

	owner.Move(movementDirection * m_moveSpeed * dt);
}
