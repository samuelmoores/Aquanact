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

	// create local movement vector with y velocity
	const glm::vec3 movement(0.0f, m_velocity.y * dt, 0.0f);

	// get this entities collider from physics world
	const ColliderHandle collider = PhysicsWorld::Instance().Find(owner);
	glm::vec3 minBounds;
	glm::vec3 maxBounds;
	owner.WorldAABB(minBounds, maxBounds);

	// compute collision for this frame
	const Physics::SweepCollision collision = PhysicsWorld::Instance().Sweep(collider, minBounds, maxBounds, movement);

	// Move normal or with a collision scalar?
	owner.Move(collision.hit ? movement * glm::clamp(collision.time, 0.0f, 1.0f) : movement);

	// if we hit something
	if (collision.hit)
	{
		glm::vec3 lastCollisionNormal = collision.normal;

		// check if we are on the ground
		// walkableGroundNormalY is 0.25
		// floor normal is 1.0
		if (collision.normal.y > walkableGroundNormalY)
		{
			m_isGrounded = true;
		}
	}

	Root::Current().Debugger().SetControllerPhysicsDiagnostics(
		owner.Name(), true, true, collision.hit, collision.time, collision.normal,
		m_velocity, m_isGrounded);

	PhysicsWorld::Instance().Update(owner);
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

	// This is intentionally a one-pass solver for now. The next step will
	// combine horizontal and vertical movement here and add iterative sliding.
	const Physics::SweepCollision collision = PhysicsWorld::Instance().Sweep(
		collider, minBounds, maxBounds, desiredMovement);
	const glm::vec3 resolvedMovement = collision.hit
		? desiredMovement * glm::clamp(collision.time, 0.0f, 1.0f)
		: desiredMovement;

	owner.Move(resolvedMovement);
	PhysicsWorld::Instance().Update(owner);
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

	// skip the collision check if we are not moving
	if (!m_isMoving)
		return;

	// get bounding box coordinates
	const ColliderHandle collider = PhysicsWorld::Instance().Find(owner);
	glm::vec3 minBounds;
	glm::vec3 maxBounds;
	owner.WorldAABB(minBounds, maxBounds);

	// multiply speed and delta time
	const glm::vec3 movement = movementDirection * m_moveSpeed * dt;

	// check for a collision
	const Physics::SweepCollision collision = PhysicsWorld::Instance().Sweep(
		collider, minBounds, maxBounds, movement);

	// move
	if(!collision.hit)
		owner.Move(movement);

	PhysicsWorld::Instance().Update(owner);
}
