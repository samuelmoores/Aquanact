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
	constexpr float groundedLossThreshold = 3.0f / 120.0f;
	constexpr float walkableGroundNormalY = 0.25f;
	constexpr float groundProbeDistance = 10.0f;

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

	std::vector<Physics::ConvexPlane> BuildConvexPlanes(Entity& object)
	{
		const Mesh* mesh = object.GetMesh();
		if (!mesh || mesh->Faces().size() < 3 || mesh->Vertices().empty())
		{
			return {};
		}

		const auto& vertices = mesh->Vertices();
		const std::size_t pointCount = std::min<std::size_t>(vertices.size(), 256);
		std::vector<glm::vec3> points;
		points.reserve(pointCount);
		for (std::size_t i = 0; i < pointCount; ++i)
		{
			const std::size_t sourceIndex = i * vertices.size() / pointCount;
			points.push_back(glm::vec3(object.BuildModelMatrix() * glm::vec4(vertices[sourceIndex].position, 1.0f)));
		}

		std::vector<glm::vec3> worldVertices;
		worldVertices.reserve(vertices.size());
		const glm::mat4 model = object.BuildModelMatrix();
		for (const Vertex3D& vertex : vertices)
		{
			worldVertices.push_back(glm::vec3(model * glm::vec4(vertex.position, 1.0f)));
		}

		std::vector<Physics::ConvexPlane> planes;
		const auto& faces = mesh->Faces();
		for (std::size_t face = 0; face + 2 < faces.size(); face += 3)
		{
			if (faces[face] >= worldVertices.size() || faces[face + 1] >= worldVertices.size() || faces[face + 2] >= worldVertices.size())
			{
				continue;
			}
			const glm::vec3 a = worldVertices[faces[face]];
			const glm::vec3 b = worldVertices[faces[face + 1]];
			const glm::vec3 c = worldVertices[faces[face + 2]];
			glm::vec3 normal = glm::cross(b - a, c - a);
			const float normalLength = glm::length(normal);
			if (normalLength <= 1e-5f)
			{
				continue;
			}
			normal /= normalLength;
			float distance = glm::dot(normal, a);
			float maximum = -std::numeric_limits<float>::max();
			float minimum = std::numeric_limits<float>::max();
			for (const glm::vec3& point : points)
			{
				const float signedDistance = glm::dot(normal, point) - distance;
				maximum = std::max(maximum, signedDistance);
				minimum = std::min(minimum, signedDistance);
			}
			if (maximum > 0.01f && minimum < -0.01f)
			{
				continue;
			}
			if (maximum > 0.01f)
			{
				normal = -normal;
				distance = -distance;
			}

			bool duplicate = false;
			for (const Physics::ConvexPlane& existing : planes)
			{
				if (glm::dot(existing.normal, normal) > 0.999f && std::abs(existing.distance - distance) < 0.01f)
				{
					duplicate = true;
					break;
				}
			}
			if (!duplicate)
			{
				planes.push_back({ normal, distance });
			}
		}
		return planes;
	}
}

void Controller::startUp(Entity&)
{
	m_velocity = glm::vec3(0.0f);
	m_grounded = false;
	m_isGrounded = false;
	m_groundedLossTimer = 0.0f;
}

std::vector<BindableMember> Controller::GetBindableMembers() const
{
	return {
		{ "IsMoving", "Is Moving", "bool", BindableMember::Kind::Function },
		{ "IsGrounded", "Is Grounded", "bool", BindableMember::Kind::Function },
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
	if (memberName == "IsGrounded")
	{
		value = m_isGrounded ? 1.0f : 0.0f;
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
			if (object->GetPhysicsColliderShape() == PhysicsColliderShape::Convex)
			{
				std::vector<Physics::ConvexPlane> planes = BuildConvexPlanes(*object);
				const glm::vec3 controllerCenter = useCapsule
					? (capsuleBase + capsuleTip) * 0.5f
					: (currentMin + currentMax) * 0.5f;
				const glm::vec3 controllerHalfExtents = (currentMax - currentMin) * 0.5f;
				const float capsuleHalfLength = useCapsule ? glm::length(capsuleTip - capsuleBase) * 0.5f : 0.0f;
				for (Physics::ConvexPlane& plane : planes)
				{
					const float support = useCapsule
						? capsuleRadius + capsuleHalfLength * std::abs(plane.normal.y)
						: glm::dot(glm::abs(plane.normal), controllerHalfExtents);
					plane.distance += support;
				}
				hit = Physics::GetConvexSweep(planes, controllerCenter, remainingMovement);
			}
			else if (useCapsule)
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
		if (collidedWithGround && earliestHit.normal.y > walkableGroundNormalY)
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
	// Horizontal movement is direct controller movement. Gravity is the only
	// continuously integrated force and applies while the controller is airborne.
	m_velocity.x = desiredHorizontalVelocity.x;
	m_velocity.z = desiredHorizontalVelocity.z;
	if (m_grounded)
	{
		m_velocity.y = 0.0f;
	}
	else
	{
		m_velocity.y = std::max(terminalFallSpeed, m_velocity.y + gravity * dt);
	}

	glm::vec3 lastCollisionNormal(0.0f);
	bool mainGroundContact = false;
	bool collidedWithGround = false;
	const glm::vec3 appliedDelta = MoveWithCollision(owner, m_velocity * dt, &lastCollisionNormal, &collidedWithGround);
	mainGroundContact = collidedWithGround;
	bool probeGroundContact = false;
	if (!collidedWithGround)
	{
		// Ramp contact can briefly miss the main sweep while descending. Probe
		// directly below the controller, then restore the probed position.
		glm::vec3 probeNormal(0.0f);
		const glm::vec3 probeDelta = MoveWithCollision(
			owner, glm::vec3(0.0f, -groundProbeDistance, 0.0f), &probeNormal, &probeGroundContact);
		if (glm::dot(probeDelta, probeDelta) > 0.0f)
		{
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
		const float velocityIntoSurface = glm::dot(m_velocity, lastCollisionNormal);
		if (velocityIntoSurface < 0.0f)
		{
			m_velocity -= lastCollisionNormal * velocityIntoSurface;
		}
	}
	if (collidedWithGround)
	{
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
