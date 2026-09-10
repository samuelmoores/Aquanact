#include "Game/AttackSphere.h"
#include "Game/Health.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SpawnManager.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <limits>

namespace
{
	glm::vec3 CubicBezier(const glm::vec3& start, const glm::vec3& controlStart,
		const glm::vec3& controlEnd, const glm::vec3& end, float progress)
	{
		const float inverse = 1.0f - progress;
		return inverse * inverse * inverse * start
			+ 3.0f * inverse * inverse * progress * controlStart
			+ 3.0f * inverse * progress * progress * controlEnd
			+ progress * progress * progress * end;
	}
}

void AttackSphere::startUp(Entity&)
{
}

bool AttackSphere::Launch(Scene& scene, Entity* ignoredEntity, float speed,
	Entity* explicitTarget, const std::string& targetBoneName,
	const std::string& targetInstanceName)
{
	if (m_launched || !Owner())
		return false;

	Entity& owner = *Owner();
	const glm::vec3 start = owner.WorldPosition();
	Entity* closest = explicitTarget;
	if (!closest)
	{
		float closestDistanceSquared = std::numeric_limits<float>::max();
		for (const auto& candidate : scene.Objects())
		{
			if (!candidate || candidate.get() == &owner || candidate.get() == ignoredEntity
				|| !candidate->GetMesh() || !candidate->GetMesh()->Skinned())
				continue;
			const glm::vec3 delta = candidate->WorldPosition() - start;
			const float distanceSquared = glm::dot(delta, delta);
			if (distanceSquared < closestDistanceSquared)
			{
				closestDistanceSquared = distanceSquared;
				closest = candidate.get();
			}
		}
	}
	if (!closest || closest == &owner || closest == ignoredEntity)
		return false;

	if (EntityStateMachine* stateMachine = owner.GetEntityState())
	{
		stateMachine->SetCurrentStateLooping(false);
		stateMachine->SetTransformAnimationEnabled(false);
	}
	// Capture the world-space launch point before changing the attachment
	// relationship. This is the authoritative position for the entire flight.
	const glm::vec3 launchWorldPosition = owner.WorldPosition();
	owner.Detach(true);
	// The detached entity is now authoritative. Use its world position as the
	// first point on the projectile path so launch cannot introduce a teleport.
	m_start = launchWorldPosition;
	owner.Translate(m_start - owner.WorldPosition());
	m_startScale = owner.Scale();
	m_end = !targetBoneName.empty() && closest->GetMesh()
		? glm::vec3(closest->BuildModelMatrix() * glm::vec4(
			closest->GetMesh()->BonePosition(targetBoneName), 1.0f))
		: closest->WorldPosition();
	const glm::vec3 delta = m_end - m_start;
	const float distance = glm::length(delta);
	const glm::vec3 direction = distance > 0.0001f
		? delta / distance : glm::vec3(0.0f, 0.0f, 1.0f);
	const float arcHeight = glm::clamp(distance * 0.25f, 2.0f, 100.0f);
	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	m_controlStart = m_start + direction * (distance * 0.33f) + up * arcHeight;
	m_controlEnd = m_end - direction * (distance * 0.33f) + up * arcHeight;
	m_elapsed = 0.0f;
	m_duration = distance / glm::max(speed, 0.001f);
	m_target = closest;
	m_scene = &scene;
	m_targetBoneName = targetBoneName;
	m_targetInstanceName = targetInstanceName;
	m_launched = true;
	m_hitTarget = false;
	return true;
}

void AttackSphere::Update(Entity& entity, float dt)
{
	if (!m_launched)
		return;

	if (!m_targetInstanceName.empty())
		m_target = Root::Current().Spawns().FindActiveInstance(*m_scene, m_targetInstanceName);
	if (m_target)
	{
		m_end = !m_targetBoneName.empty() && m_target->GetMesh()
			? glm::vec3(m_target->BuildModelMatrix() * glm::vec4(
				m_target->GetMesh()->BonePosition(m_targetBoneName), 1.0f))
			: m_target->WorldPosition();

		// Re-read the target bone every frame and home from the sphere's current
		// world position. Rebuilding the original launch curve only changes its
		// endpoint; it does not reliably follow a target that moves through the
		// level. This keeps the launch smooth while allowing the projectile to
		// continuously steer toward the animated bone.
		const glm::vec3 currentPosition = entity.WorldPosition();
		const glm::vec3 toTarget = m_end - currentPosition;
		const float distance = glm::length(toTarget);
		const glm::vec3 direction = distance > 0.0001f
			? toTarget / distance : glm::vec3(0.0f, 0.0f, 1.0f);
		const float arcHeight = glm::clamp(distance * 0.25f, 2.0f, 100.0f);
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		m_controlStart = m_start + direction * (distance * 0.33f) + up * arcHeight;
		m_controlEnd = m_end - direction * (distance * 0.33f) + up * arcHeight;
	}

	m_elapsed = glm::min(m_elapsed + glm::max(dt, 0.0f), m_duration);
	const float progress = m_duration > 0.0001f ? m_elapsed / m_duration : 1.0f;
	const glm::vec3 desiredPosition = CubicBezier(
		m_start, m_controlStart, m_controlEnd, m_end, progress);
	entity.Translate(desiredPosition - entity.WorldPosition());
	entity.SetScale(glm::mix(m_startScale, m_startScale * 3.0f, progress));
	PhysicsWorld::Instance().Update(entity);

	// Damage only the configured homing target. World bounds are refreshed after
	// movement and scaling above, so contact follows the sphere's visible size.
	if (!m_hitTarget && m_target)
	{
		glm::vec3 sphereMin;
		glm::vec3 sphereMax;
		glm::vec3 targetMin;
		glm::vec3 targetMax;
		if (entity.WorldAABB(sphereMin, sphereMax)
			&& m_target->WorldAABB(targetMin, targetMax)
			&& sphereMin.x <= targetMax.x && sphereMax.x >= targetMin.x
			&& sphereMin.y <= targetMax.y && sphereMax.y >= targetMin.y
			&& sphereMin.z <= targetMax.z && sphereMax.z >= targetMin.z)
		{
			if (Health* health = m_target->GetComponent<Health>())
				health->ReceiveDamage(entity, health->CurrentHealth());
			m_hitTarget = true;
		}
	}
}

// Keep component metadata in the header with the binding/event list macros.
// Add runtime logic here only if the component needs it.
