#include "Engine/Core/Controller.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Physics.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/FrameProfiler.h"

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

bool Controller::IsWalkableSurface(const glm::vec3& normal) const
{
	const float minimumNormalY = std::cos(glm::radians(MaxWalkableSlopeAngle()));
	return normal.y >= minimumNormalY;
}

void Controller::startUp(Entity&)
{
	m_velocity = glm::vec3(0.0f);
	m_grounded = false;
}

void Controller::Update(Entity& owner, float dt)
{
	// is the game running
	if (dt <= 0.0f)
	{
		return;
	}

	// Preserve the previous contact only while resolving the next movement step.
	// A jump explicitly clears m_grounded before this function, so this does not
	// suppress the launch impulse.
	// Grounded is a yes/no result of this frame's collision solve.
	m_grounded = false;

	m_velocity.y = std::max(
		m_velocity.y + gravity * GravityScale() * dt,
		terminalFallSpeed);

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
	FrameProfiler::Scope movementScope(Root::Current().Profiler(), "Controller Movement");
	if (dt <= 0.0f || glm::length(desiredMovement) <= 0.0001f)
	{
		return;
	}

	const ColliderHandle collider = PhysicsWorld::Instance().Find(owner);
	glm::vec3 minBounds;
	glm::vec3 maxBounds;
	bool verticalSweepHit = false;
	float verticalSweepTime = 1.0f;
	glm::vec3 verticalSweepNormal(0.0f);
	if (collider == InvalidColliderHandle || !owner.WorldAABB(minBounds, maxBounds))
	{
		owner.Move(desiredMovement);
		Root::Current().Debugger().SetControllerPhysicsDiagnostics(
			owner.Name(), collider != InvalidColliderHandle, false, false, 1.0f,
			glm::vec3(0.0f), m_velocity, false);
		Root::Current().Debugger().LogTagged(
			Debug::Severity::Warning, "Controller",
			owner.Name() + " has no usable physics collider or world bounds");
		return;
	}

	constexpr int maxSlideIterations = 4;
	// Keep subsequent sweeps slightly separated from the contacted surface so
	// slopes do not repeatedly report the same zero-time collision.
	constexpr float collisionSkin = 0.1f;
	glm::vec3 remainingMovement = desiredMovement;
	glm::vec3 resolvedMovement(0.0f);

	for (int iteration = 0; iteration < maxSlideIterations; ++iteration)
	{
		if (glm::length(remainingMovement) <= 0.0001f)
		{
			break;
		}

		Entity* hitEntity = nullptr;
		const PhysicsCollider* hitCollider = nullptr;
		const glm::vec3 movementBeforeSweep = remainingMovement;
		const glm::vec3 resolvedBeforeSweep = resolvedMovement;
		const Physics::SweepCollision collision = PhysicsWorld::Instance().SweepEntity(
			owner, remainingMovement, resolvedMovement, &hitEntity, &hitCollider);

		if (!collision.hit)
		{
			resolvedMovement += remainingMovement;
			break;
		}
		if (remainingMovement.y < 0.0f)
		{
			verticalSweepHit = true;
			verticalSweepTime = collision.time;
			verticalSweepNormal = collision.normal;
		}

		const bool walkableSurface = IsWalkableSurface(collision.normal);
		const bool collidedWithGround = remainingMovement.y <= 0.0f && walkableSurface;
		const bool collidedWithCeiling = remainingMovement.y > 0.0f
			&& IsWalkableSurface(-collision.normal);

		m_groundNormal = collision.normal;

		if (collidedWithGround)
		{
			m_grounded = true;
			m_velocity.y = 0.0f;
		}
		else if (collidedWithCeiling)
		{
			// Stop the jump impulse on a downward-facing walkable contact. Movement
			// was already projected off the ceiling; retaining upward velocity would
			// keep driving the controller into it on subsequent frames.
			m_velocity.y = 0.0f;
		}

		const glm::vec3 previousResolvedMovement = resolvedMovement;
		remainingMovement = ResolveSlideCollision(
			collision, remainingMovement, resolvedMovement, collisionSkin);
		if (collision.time <= 0.0001f)
		{
			// Separate from an existing contact before the next sweep. This is
			// especially important where a slope transitions into flat ground.
			resolvedMovement += collision.normal * collisionSkin;
		}
		if (walkableSurface && m_isMoving)
		{
			remainingMovement = ProjectMovementOntoWalkableGround(
				remainingMovement, collision.normal, MaxWalkableSlopeAngle());
		}
		else if (collidedWithGround)
		{
			// Do not turn gravity into downhill locomotion when the player has no
			// movement input. Keep the controller planted on the slope.
			remainingMovement = glm::vec3(0.0f);
		}

		if (hitEntity && hitCollider
			&& hitCollider->shape == PhysicsColliderShape::Convex)
		{
			const auto formatVector = [](const glm::vec3& value)
			{
				return "(" + std::to_string(value.x) + ","
					+ std::to_string(value.y) + ","
					+ std::to_string(value.z) + ")";
			};
			const std::string colliderPart = hitCollider->isBaseMeshCollider
				? "base-mesh" : "submesh-" + std::to_string(hitCollider->submeshIndex);
			Root::Current().Debugger().LogConvexCollision(
				"controller|" + owner.Name() + "|" + hitEntity->Name() + "|" + colliderPart,
				"Controller contact moving=" + owner.Name()
				+ " target=" + hitEntity->Name()
				+ " collider=" + colliderPart
				+ " planes=" + std::to_string(hitCollider->convexPlanes.size())
				+ " parts=" + std::to_string(hitCollider->convexParts.size())
				+ " fallback=" + (hitCollider->convexPlanes.empty() && hitCollider->convexParts.empty() ? "AABB" : "none")
				+ " planeSource=" + (hitCollider->convexParts.size() > 1 ? "thin-plane-compound"
					: hitCollider->convexFromThinPlane ? "thin-plane-hull"
					: hitCollider->convexPlanes.empty() ? "unavailable" : "mesh-faces")
				+ " iteration=" + std::to_string(iteration)
				+ " time=" + std::to_string(collision.time)
				+ " normal=" + formatVector(collision.normal)
				+ " desired=" + formatVector(desiredMovement)
				+ " sweepMovement=" + formatVector(movementBeforeSweep)
				+ " resolvedBefore=" + formatVector(resolvedBeforeSweep)
				+ " resolvedAfter=" + formatVector(resolvedMovement)
				+ " remainingAfter=" + formatVector(remainingMovement));
		}

		// Avoid repeatedly resolving a zero-time contact that produces no useful
		// change, which can otherwise consume every iteration at a corner.
		const bool madeNoProgress = glm::length(resolvedMovement - previousResolvedMovement) <= 0.0001f;
		if (madeNoProgress && glm::length(remainingMovement) <= 0.0001f)
		{
			break;
		}
	}

	owner.Move(resolvedMovement);
	PhysicsWorld::Instance().Update(owner);

	// Keep the controller attached to nearby walkable surfaces when moving
	// downhill or across small gaps in collision geometry.
	if (m_velocity.y <= 0.0f && !m_grounded)
	{
		const glm::vec3 probeMovement(0.0f, -groundProbeDistance, 0.0f);
		const Physics::SweepCollision probe = PhysicsWorld::Instance().SweepEntity(
			owner, probeMovement);

		if (probe.hit && IsWalkableSurface(probe.normal))
		{
			verticalSweepHit = true;
			verticalSweepTime = probe.time;
			verticalSweepNormal = probe.normal;
			owner.Move(probeMovement * glm::clamp(probe.time, 0.0f, 1.0f));
			m_grounded = true;
			m_groundNormal = probe.normal;
			m_velocity.y = 0.0f;
			PhysicsWorld::Instance().Update(owner);
		}
	}

	Root::Current().Debugger().SetControllerPhysicsDiagnostics(
		owner.Name(), true, true, verticalSweepHit, verticalSweepTime,
		verticalSweepNormal, m_velocity, m_grounded);
	Root::Current().Debugger().SetGameplayDiagnostics(
		owner.Name(), m_diagnosticInput, m_moveSpeed, dt, resolvedMovement,
		owner.Position(), GroundSurfaceAngle(), m_grounded);
}
