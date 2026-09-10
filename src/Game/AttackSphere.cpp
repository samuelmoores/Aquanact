#include "Game/AttackSphere.h"
#include "Game/Health.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SpawnManager.h"
#include "Game/PlayerController.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <limits>

namespace
{
	constexpr float finalScaleMultiplier = 4.0f;

	glm::vec3 CubicBezier(const glm::vec3& start, const glm::vec3& controlStart,
		const glm::vec3& controlEnd, const glm::vec3& end, float progress)
	{
		const float inverse = 1.0f - progress;
		return inverse * inverse * inverse * start
			+ 3.0f * inverse * inverse * progress * controlStart
			+ 3.0f * inverse * progress * progress * controlEnd
			+ progress * progress * progress * end;
	}

	glm::vec3 BoneWorldPosition(Entity& entity, const std::string& boneName)
	{
		if (boneName.empty() || !entity.GetMesh())
			return entity.WorldPosition();
		return glm::vec3(entity.BuildModelMatrix() * glm::vec4(
			entity.GetMesh()->BonePosition(boneName), 1.0f));
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
	m_speed = glm::max(speed, 0.001f);
	m_target = closest;
	m_player = ignoredEntity;
	m_scene = &scene;
	m_targetBoneName = targetBoneName;
	m_targetInstanceName = targetInstanceName;
	m_launched = true;
	m_hitTarget = false;
	m_observedHurtAnimation = false;
	m_travelPhase = TravelPhase::Outbound;
	return true;
}

void AttackSphere::Update(Entity& entity, float dt)
{
	if (!m_launched)
		return;

	if (m_travelPhase == TravelPhase::AtPlayerHead)
	{
		if (m_player)
			entity.Translate(BoneWorldPosition(*m_player, m_playerBoneName) - entity.WorldPosition());
		entity.SetScale(m_startScale * finalScaleMultiplier);
		PhysicsWorld::Instance().Update(entity);
		return;
	}

	if (m_travelPhase == TravelPhase::WaitingForHurt)
	{
		if (m_target)
			entity.Translate(BoneWorldPosition(*m_target, m_targetBoneName) - entity.WorldPosition());
		entity.SetScale(m_startScale * finalScaleMultiplier);
		bool hurtAnimationComplete = false;
		if (m_target)
		{
			if (EntityStateMachine* targetState = m_target->GetEntityState())
			{
				const bool playingHurt = targetState->CurrentState() == "hurt"
					|| targetState->CurrentState() == "Hurt";
				if (playingHurt)
				{
					m_observedHurtAnimation = true;
					int hurtClipIndex = -1;
					for (const EntityStateMachine::State& state : targetState->States())
					{
						if (state.name == targetState->CurrentState())
						{
							hurtClipIndex = targetState->FindAnimationIndex(state.animationName);
							break;
						}
					}
					const Animator* animator = targetState->GetAnimator();
					const bool hurtClipActive = !animator || hurtClipIndex < 0
						|| animator->CurrentClipIndex() == hurtClipIndex;
					if (hurtClipActive)
					{
						const float duration = animator && hurtClipIndex >= 0
							? animator->ClipDuration(hurtClipIndex)
							: targetState->CurrentStateClipDurationSeconds();
						hurtAnimationComplete = duration <= 0.0f
							|| targetState->CurrentStateElapsedSeconds() >= duration;
					}
				}
				else if (m_observedHurtAnimation)
				{
					// A transition away from hurt also means its authored reaction has ended.
					hurtAnimationComplete = true;
				}
			}
			else
			{
				hurtAnimationComplete = true;
			}
		}
		if (hurtAnimationComplete && m_player)
		{
			m_start = entity.WorldPosition();
			m_end = BoneWorldPosition(*m_player, m_playerBoneName);
			const glm::vec3 delta = m_end - m_start;
			const float distance = glm::length(delta);
			const glm::vec3 direction = distance > 0.0001f
				? delta / distance : glm::vec3(0.0f, 0.0f, 1.0f);
			const float arcHeight = glm::clamp(distance * 0.25f, 2.0f, 100.0f);
			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			m_controlStart = m_start + direction * (distance * 0.33f) + up * arcHeight;
			m_controlEnd = m_end - direction * (distance * 0.33f) + up * arcHeight;
			m_elapsed = 0.0f;
			m_duration = distance / m_speed;
			m_travelPhase = TravelPhase::ReturningToPlayer;
		}
		PhysicsWorld::Instance().Update(entity);
		return;
	}

	Entity* travelTarget = m_travelPhase == TravelPhase::ReturningToPlayer
		? m_player : m_target;
	const std::string& travelBoneName = m_travelPhase == TravelPhase::ReturningToPlayer
		? m_playerBoneName : m_targetBoneName;
	if (m_travelPhase == TravelPhase::Outbound && !m_targetInstanceName.empty())
	{
		m_target = Root::Current().Spawns().FindActiveInstance(*m_scene, m_targetInstanceName);
		travelTarget = m_target;
	}
	if (travelTarget)
	{
		m_end = BoneWorldPosition(*travelTarget, travelBoneName);

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
	entity.SetScale(m_travelPhase == TravelPhase::Outbound
		? glm::mix(m_startScale, m_startScale * finalScaleMultiplier, progress)
		: m_startScale * finalScaleMultiplier);
	PhysicsWorld::Instance().Update(entity);
	if (m_travelPhase == TravelPhase::ReturningToPlayer && progress >= 1.0f)
	{
		if (m_player)
		{
			if (PlayerController* playerController = m_player->GetComponent<PlayerController>())
			{
				playerController->SetMovementLocked(true);
				if (EntityStateMachine* playerState = m_player->GetEntityState())
					playerState->SetDesiredState("death");
			}
		}
		m_travelPhase = TravelPhase::AtPlayerHead;
		return;
	}
	// Damage only after the interpolation has reached the configured target bone.
	// The sphere is intentionally large, so an AABB overlap would trigger early
	// and make the projectile appear to stop short before snapping into place.
	if (m_travelPhase == TravelPhase::Outbound && !m_hitTarget && m_target)
	{
		const glm::vec3 targetPosition = BoneWorldPosition(*m_target, m_targetBoneName);
		const float targetDistance = glm::length(targetPosition - entity.WorldPosition());
		if (progress >= 1.0f && targetDistance <= 1.0f)
		{
			// Remove any accumulated floating-point error before beginning the hurt
			// pause. This is now an imperceptible correction at the destination.
			entity.Translate(targetPosition - entity.WorldPosition());
			if (Health* health = m_target->GetComponent<Health>())
				health->ReceiveDamage(entity, health->CurrentHealth());
			entity.SetScale(m_startScale * finalScaleMultiplier);
			m_hitTarget = true;
			m_observedHurtAnimation = false;
			m_travelPhase = TravelPhase::WaitingForHurt;
		}
	}
}

// Keep component metadata in the header with the binding/event list macros.
// Add runtime logic here only if the component needs it.
