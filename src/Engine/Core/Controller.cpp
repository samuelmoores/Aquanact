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

	void RecordCollisionContact(const Physics::SweepCollision& collision,
		glm::vec3* lastCollisionNormal, bool* collidedWithGround)
	{
		// Preserve the latest contact for grounding and velocity response. The
		// world has already selected the earliest hit for this slide iteration.
		if (lastCollisionNormal)
		{
			*lastCollisionNormal = collision.normal;
		}
		if (collidedWithGround && collision.normal.y > walkableGroundNormalY)
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

void Controller::startUp(Entity&)
{
	// Reset all runtime movement state when the component is attached or the
	// entity is initialized. Editor/configuration values are left unchanged.
	m_velocity = glm::vec3(0.0f);
	m_grounded = false;
	m_isGrounded = false;
	m_groundedLossTimer = 0.0f;
}

void Controller::SetMovementDirection(const glm::vec3& direction)
{
	// Store the current movement intent and the same value used by diagnostics.
	// Normalization and world-speed conversion happen during ApplyMovement().
	m_movementDirection = direction;
	m_diagnosticInput = direction;
}

void Controller::StopMoving()
{
	// Clear both gameplay input and its diagnostic snapshot so stopping is
	// reflected consistently in movement and debugging views.
	m_movementDirection = glm::vec3(0.0f);
	m_diagnosticInput = glm::vec3(0.0f);
}

glm::vec3 Controller::MoveWithCollision(Entity& owner, const glm::vec3& delta,
	glm::vec3* lastCollisionNormal, bool* collidedWithGround)
{
	// Outputs describe only this movement attempt, so reset them before any
	// early return or collision query.
	if (lastCollisionNormal)
	{
		*lastCollisionNormal = glm::vec3(0.0f);
	}

	// assume we are not on the ground
	if (collidedWithGround)
	{
		*collidedWithGround = false;
	}

	// check if we are moving
	if (glm::length(delta) <= 0.0001f)
	{
		return glm::vec3(0.0f);
	}

	// PhysicsWorld stores the collider snapshot. The entity AABB is still read
	// here as the starting shape for this movement step.
	glm::vec3 startMin;
	glm::vec3 startMax;

	const ColliderHandle ownerCollider = PhysicsWorld::Instance().Find(owner);

	// Collision resolution requires both a registered world collider and a
	// valid current AABB. If either is unavailable, use direct movement rather
	// than attempting a sweep with incomplete shape data.
	if (ownerCollider == InvalidColliderHandle || !owner.WorldAABB(startMin, startMax))
	{
		// Without a registered collider or valid bounds, preserve the old safe
		// fallback: apply movement directly rather than dropping the input.
		owner.Move(delta);
		return delta;
	}

	glm::vec3 resolvedDelta(0.0f);
	glm::vec3 remainingMovement = delta;
	constexpr int maxSlideIterations = 4;

	// Controller contacts are resolved directly to the surface. Keeping a
	// deliberate gap here causes the capsule to visibly float after landing.
	constexpr float collisionSkin = 0.0f;
	for (int iteration = 0; 
		(iteration < maxSlideIterations) && (glm::length(remainingMovement) > 0.0001f); 
		++iteration)
	{
		// Rebuild the current bounds from the original bounds plus the movement
		// already accepted during earlier slide iterations.
		const glm::vec3 currentMin = startMin + resolvedDelta;
		const glm::vec3 currentMax = startMax + resolvedDelta;

		// PhysicsWorld owns candidate iteration, broadphase filtering, and shape
		// dispatch. The controller only consumes the earliest contact.
		const Physics::SweepCollision earliestHit = PhysicsWorld::Instance().Sweep(
			ownerCollider, currentMin, currentMax, remainingMovement);

		if (!earliestHit.hit)
		{
			// No surface blocks the remaining movement, so accept it completely.
			resolvedDelta += remainingMovement;
			break;
		}

		RecordCollisionContact(earliestHit, lastCollisionNormal, collidedWithGround);

		remainingMovement = ResolveSlideCollision(
			earliestHit, remainingMovement, resolvedDelta, collisionSkin);
	}

	// Apply the accumulated movement once, after all slide iterations finish.
	owner.Move(resolvedDelta);
	return resolvedDelta;
}

glm::vec3 Controller::MoveWithPhysics(Entity& owner, const glm::vec3& desiredHorizontalVelocity, float dt)
{
	// Convert horizontal input into the controller's current velocity. Gravity is
	// the only continuously integrated force in this kinematic controller.
	m_velocity.x = desiredHorizontalVelocity.x;
	m_velocity.z = desiredHorizontalVelocity.z;
	if (m_grounded)
	{
		m_velocity.y = 0.0f;
	}
	else
	{
		m_velocity.y = std::max(
			terminalFallSpeed,
			m_velocity.y + gravity * GravityScale() * dt);
	}

	// Resolve the requested velocity through the world and collect contact data
	// for grounding and velocity response.
	glm::vec3 lastCollisionNormal(0.0f);
	bool mainGroundContact = false;
	bool collidedWithGround = false;

	const glm::vec3 appliedDelta = MoveWithCollision(owner, m_velocity * dt, &lastCollisionNormal, &collidedWithGround);
	mainGroundContact = collidedWithGround;

	bool probeGroundContact = false;

	// Do not run the downward ground probe while moving upward. During a jump,
	// probing from the newly raised position can find the floor and snap the
	// capsule back down in the same frame that the jump was applied.
	if (!collidedWithGround && m_velocity.y <= 0.0f)
	{
		// Ramp contact can briefly miss the main sweep while descending. Probe
		// directly below the controller, then restore the probed position.
		glm::vec3 probeNormal(0.0f);
		const glm::vec3 probeDelta = MoveWithCollision(
			owner, glm::vec3(0.0f, -groundProbeDistance, 0.0f), &probeNormal, &probeGroundContact);
		if (!probeGroundContact && glm::dot(probeDelta, probeDelta) > 0.0f)
		{
			// Restore probes that hit a wall or another non-ground surface. A
			// walkable ground hit is intentionally retained to snap onto the floor.
			owner.Move(-probeDelta);
		}
		if (probeGroundContact)
		{
			collidedWithGround = true;
			lastCollisionNormal = probeNormal;
		}
	}
	if (glm::dot(lastCollisionNormal, lastCollisionNormal) > 0.0f)
	{
		// Remove only velocity directed into the contacted surface. Tangential
		// velocity is preserved so the controller can continue sliding.
		const float velocityIntoSurface = glm::dot(m_velocity, lastCollisionNormal);
		if (velocityIntoSurface < 0.0f)
		{
			m_velocity -= lastCollisionNormal * velocityIntoSurface;
		}
	}
	if (collidedWithGround)
	{
		// Contact with a walkable surface establishes grounded state and cancels
		// downward velocity. The transition event fires only on state changes.
		const bool wasGrounded = m_isGrounded;
		m_grounded = true;
		m_isGrounded = true;
		m_groundedLossTimer = 0.0f;
		m_velocity.y = 0.0f;
		if (!wasGrounded)
		{
			Root::Current().Debugger().RecordGroundedTransition(owner.Name(), true, m_grounded,
				mainGroundContact, probeGroundContact, lastCollisionNormal, owner.Position(), m_velocity,
				m_groundedLossTimer, dt);
		}
	}
	else
	{
		// A brief missed contact should not immediately produce a falling state;
		// the timer filters out small gaps caused by seams or uneven geometry.
		m_grounded = false;
		m_groundedLossTimer += dt;
		if (m_groundedLossTimer >= groundedLossThreshold)
		{
			if (m_isGrounded)
			{
				m_isGrounded = false;
				Root::Current().Debugger().RecordGroundedTransition(owner.Name(), false, m_grounded,
					mainGroundContact, probeGroundContact, lastCollisionNormal, owner.Position(), m_velocity,
					m_groundedLossTimer, dt);
			}
		}
	}
	return appliedDelta;
}

void Controller::Update(Entity& owner, float dt)
{
	// Component entry point. Input/state code sets movement intent before this
	// update, and ApplyMovement converts that intent into physics motion.
	ApplyMovement(owner, dt);
}

void Controller::ApplyMovement(Entity& owner, float dt)
{
	// Convert the requested direction into a horizontal velocity and orient the
	// entity toward travel direction. Vertical motion is handled separately by
	// MoveWithPhysics().
	glm::vec3 desiredHorizontalVelocity(0.0f);
	if (glm::length(m_movementDirection) > 0.0f)
	{
		const glm::vec3 movement = glm::normalize(m_movementDirection);
		m_isMoving = true;
		const float targetYaw = std::atan2(movement.x, movement.z);
		owner.SetRotation(glm::vec3(owner.Rotation().x, targetYaw, owner.Rotation().z));
		desiredHorizontalVelocity = movement * m_moveSpeed;
	}
	else
	{
		// No input means stop horizontal movement while preserving vertical state.
		m_isMoving = false;
	}

	// Apply movement through collision resolution, then publish the resulting
	// displacement to the diagnostics system.
	const glm::vec3 appliedDelta = MoveWithPhysics(owner, desiredHorizontalVelocity, dt);
	Root::Current().Debugger().SetGameplayDiagnostics(owner.Name(), m_diagnosticInput, m_moveSpeed, dt, appliedDelta, owner.Position());
}
