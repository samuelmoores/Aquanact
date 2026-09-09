#include "Engine/Core/Hitbox.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/PhysicsWorld.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
	std::string Lowercase(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}

	int CombatTeam(Entity& entity)
	{
		for (Component* component : entity.Components())
		{
			if (component && component->TeamId() != 0)
				return component->TeamId();
		}
		return 0;
	}
}

Hitbox::~Hitbox()
{
	if (Owner() && m_collider != InvalidColliderHandle)
		PhysicsWorld::Instance().RemoveHitbox(m_collider, *Owner());
}

void Hitbox::SetRadius(float radius)
{
	if (std::isfinite(radius))
		m_radius = std::max(0.001f, radius);
}

void Hitbox::SetDamage(float damage)
{
	if (std::isfinite(damage))
		m_damage = std::max(0.0f, damage);
}

void Hitbox::SetActiveStart(float normalizedTime)
{
	if (std::isfinite(normalizedTime))
	{
		m_activeStart = std::clamp(normalizedTime, 0.0f, 1.0f);
		m_activeEnd = std::max(m_activeEnd, m_activeStart);
	}
}

void Hitbox::SetActiveEnd(float normalizedTime)
{
	if (std::isfinite(normalizedTime))
	{
		m_activeEnd = std::clamp(normalizedTime, 0.0f, 1.0f);
		m_activeStart = std::min(m_activeStart, m_activeEnd);
	}
}

void Hitbox::BeginAttack()
{
	m_active = false;
	m_hitTargets.clear();
}

void Hitbox::EndAttack()
{
	m_active = false;
}

glm::vec3 Hitbox::DebugCenter() const
{
	Entity* owner = Owner();
	if (!owner || !owner->GetMesh())
		return glm::vec3(0.0f);

	if (m_boneName.empty())
		return owner->WorldCenterPosition();

	const glm::vec3 bonePosition = owner->GetMesh()->BonePosition(m_boneName);
	return glm::vec3(owner->BuildModelMatrix() * glm::vec4(bonePosition, 1.0f));
}

void Hitbox::FirstFrame(Entity& owner)
{
	Sync(owner);
}

void Hitbox::Update(Entity& owner, float)
{
	Sync(owner);
	UpdateAttackWindow(owner);
	if (m_active)
		RegisterHits(owner);
}

void Hitbox::Sync(Entity& owner)
{
	if (!owner.GetMesh()) return;

	const glm::vec3 center = DebugCenter();
	const glm::vec3 halfExtents(m_radius);
	if (m_collider == InvalidColliderHandle
		|| !PhysicsWorld::Instance().UpdateHitbox(m_collider, owner, m_shape, center, halfExtents))
		m_collider = PhysicsWorld::Instance().AddHitbox(owner, m_shape, center, halfExtents);
}

void Hitbox::UpdateAttackWindow(Entity& owner)
{
	EntityStateMachine* stateMachine = owner.GetComponent<EntityStateMachine>();
	if (!stateMachine)
	{
		EndAttack();
		return;
	}

	const std::string currentState = stateMachine->CurrentState();
	const float elapsed = stateMachine->CurrentStateElapsedSeconds();
	const bool stateRestarted = currentState != m_lastAnimationState
		|| elapsed + 0.0001f < m_lastStateElapsed;
	const bool isAttackState = Lowercase(currentState) == "attack";
	if (stateRestarted)
	{
		if (isAttackState)
			BeginAttack();
		else
			EndAttack();
	}

	const float duration = stateMachine->CurrentStateClipDurationSeconds();
	const float normalizedTime = duration > 0.0f ? elapsed / duration : 0.0f;
	m_active = isAttackState
		&& normalizedTime >= m_activeStart
		&& normalizedTime <= m_activeEnd;
	m_lastAnimationState = currentState;
	m_lastStateElapsed = elapsed;
}

void Hitbox::RegisterHits(Entity& owner)
{
	if (m_collider == InvalidColliderHandle || m_damage <= 0.0f)
		return;

	const int ownerTeam = CombatTeam(owner);
	for (Entity* target : PhysicsWorld::Instance().QueryHitbox(m_collider, &owner))
	{
		if (!target || m_hitTargets.contains(target))
			continue;
		const int targetTeam = CombatTeam(*target);
		if (ownerTeam != 0 && targetTeam != 0 && ownerTeam == targetTeam)
			continue;

		for (Component* component : target->Components())
		{
			if (component && component->ReceiveDamage(owner, m_damage))
			{
				m_hitTargets.insert(target);
				DispatchBindableEvent("HitConfirmed");
				break;
			}
		}
	}
}
