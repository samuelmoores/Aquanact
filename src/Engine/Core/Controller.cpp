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
		const glm::vec3& movement, glm::vec3* lastCollisionNormal,
		bool* collidedWithGround)
	{
		if (lastCollisionNormal)
			*lastCollisionNormal = collision.normal;
		if (collidedWithGround && movement.y <= 0.0f &&
			collision.normal.y > walkableGroundNormalY)
			*collidedWithGround = true;
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
	m_isGrounded = false;
}

void Controller::Update(Entity& owner, float dt)
{
	// is the game running
	if (dt <= 0.0f)
	{
		return;
	}

	m_isGrounded = false;
	m_velocity.y = gravity * GravityScale();
	m_pendingMovement = glm::vec3(0.0f, m_velocity.y * dt, 0.0f);
}

void Controller::Move(Entity& owner, const glm::vec2& direction, float dt)
{
	// create input magnitude
	const float inputLength = glm::length(direction);

	// Keep diagonal input from moving faster than cardinal input.
	const glm::vec2 clampedDirection = inputLength > 1.0f ? direction / inputLength : direction;

	// create movement vector
	const glm::vec3 movementDirection(clampedDirection.x, 0.0f, clampedDirection.y);

	// are we moving?
	m_isMoving = glm::length(movementDirection) > 0.0001f;

	// normalize or set to zero
	m_movementDirection = m_isMoving ? glm::normalize(movementDirection) : glm::vec3(0.0f);

	SetDiagnosticInput(movementDirection);

	if (dt <= 0.0f)
		return;

	const glm::vec3 movement = m_pendingMovement + movementDirection * m_moveSpeed * dt;
	MoveWithCollisions(owner, movement, dt);
	m_pendingMovement = glm::vec3(0.0f);
}

void Controller::MoveWithCollisions(Entity& owner, const glm::vec3& desiredMovement, float dt)
{
	if (dt <= 0.0f || glm::length(desiredMovement) <= 0.0001f)
	{
		return;
	}

	const ColliderHandle collider = PhysicsWorld::Instance().Find(owner);
	glm::vec3 minBounds;
	glm::vec3 maxBounds;
	if (collider == InvalidColliderHandle || !owner.WorldAABB(minBounds, maxBounds))
	{
		owner.Move(desiredMovement);
		return;
	}

	constexpr int maxSlideIterations = 4;
	constexpr float collisionSkin = 0.0f;
	glm::vec3 remainingMovement = desiredMovement;
	glm::vec3 resolvedMovement(0.0f);

	for (int iteration = 0; iteration < maxSlideIterations; ++iteration)
	{
		if (glm::length(remainingMovement) <= 0.0001f)
		{
			break;
		}

		const Physics::SweepCollision collision = PhysicsWorld::Instance().Sweep(
			collider,
			minBounds + resolvedMovement,
			maxBounds + resolvedMovement,
			remainingMovement);

		if (!collision.hit)
		{
			resolvedMovement += remainingMovement;
			break;
		}

		bool collidedWithGround = false;
		RecordCollisionContact(
			collision, remainingMovement, &m_groundNormal, &collidedWithGround);
		if (collidedWithGround)
		{
			m_grounded = true;
			m_isGrounded = true;
			m_groundedLossTimer = 0.0f;
			m_velocity.y = 0.0f;
		}

		const glm::vec3 previousResolvedMovement = resolvedMovement;
		remainingMovement = ResolveSlideCollision(
			collision, remainingMovement, resolvedMovement, collisionSkin);

		// Avoid repeatedly resolving a zero-time contact that produces no useful
		// change, which can otherwise consume every iteration at a corner.
		if (glm::length(resolvedMovement - previousResolvedMovement) <= 0.0001f &&
			glm::length(remainingMovement) >= glm::length(desiredMovement) - 0.0001f)
		{
			break;
		}
	}

	owner.Move(resolvedMovement);
	PhysicsWorld::Instance().Update(owner);
}
