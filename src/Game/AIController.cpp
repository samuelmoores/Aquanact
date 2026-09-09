#include "Game/AIController.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/EventManager.h"
#include "Engine/Core/MathUtils.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Game/PlayerController.h"
#include "Game/Health.h"

#include <algorithm>
#include <cmath>

void AIController::startUp(Entity& owner)
{
	Controller::startUp(owner);
	// Hostile actors need a damage receiver for attack hitboxes. Instance
	// definitions may add Health explicitly; otherwise provide the default.
	if (!owner.GetComponent<Health>())
		owner.AddComponent<Health>();
	m_target = nullptr;
	m_state = State::Idle;
	m_attackCooldownRemaining = 0.0f;
	m_hitTrigger = false;
	m_pendingHit = false;
}

void AIController::FirstFrame(Entity& owner)
{
	m_entityState = owner.GetComponent<EntityStateMachine>();
	if (Health* health = owner.GetComponent<Health>())
	{
		Root::Current().Events().GetEvent(health->BindableEventChannel("DamageTaken"))
			.Subscribe(this, [this]()
			{
				m_pendingHit = true;
			});
	}
	FindTarget();
}

void AIController::FindTarget()
{
	m_target = nullptr;
	if (const Scene* scene = Root::Current().Scenes().ActiveLevel())
	{
		for (const auto& object : scene->Objects())
		{
			if (object && object->GetComponent<PlayerController>())
			{
				m_target = object.get();
				return;
			}
		}
	}
}

void AIController::SetState(State state)
{
	if (m_state == state)
		return;
	m_state = state;
	if (state == State::Idle) SetAnimationState("Idle");
	else if (state == State::Chase) SetAnimationState("Walk");
	else SetAnimationState("Attack");
}

void AIController::SetAnimationState(const char* stateName)
{
	if (m_entityState)
		m_entityState->SetDesiredState(stateName);
}

void AIController::FaceMovement(Entity& owner, const glm::vec3& direction, float dt)
{
	if (m_turnSpeed <= 0.0f || glm::length(direction) <= 0.0001f)
		return;
	const float targetYaw = std::atan2(direction.x, direction.z);
	const float currentYaw = owner.Rotation().y;
	const float delta = MathUtils::ShortestAngleDelta(currentYaw, targetYaw);
	const float step = std::max(0.0f, m_turnSpeed) * std::max(0.0f, dt);
	owner.SetRotation(glm::vec3(owner.Rotation().x, currentYaw + std::clamp(delta, -step, step), owner.Rotation().z));
}

void AIController::Update(Entity& owner, float dt)
{
	// Hitbox processing runs after the controller and state machine. Promote the
	// previous frame's damage event now so state transitions can test WasHit().
	m_hitTrigger = m_pendingHit;
	m_pendingHit = false;

	Controller::Update(owner, dt);
	if (dt <= 0.0f)
		return;

	if (!m_target)
		FindTarget();
	if (!m_target)
	{
		m_targetDistance = 0.0f;
		SetState(State::Idle);
		Controller::Move(owner, glm::vec2(0.0f), dt);
		return;
	}

	glm::vec3 offset = m_target->WorldCenterPosition() - owner.WorldCenterPosition();
	offset.y = 0.0f;
	m_targetDistance = glm::length(offset);
	if (m_targetDistance > m_detectionRange)
	{
		m_target = nullptr;
		SetState(State::Idle);
		Controller::Move(owner, glm::vec2(0.0f), dt);
		return;
	}

	m_attackCooldownRemaining = std::max(0.0f, m_attackCooldownRemaining - dt);
	if (m_targetDistance <= m_attackRange)
	{
		SetState(State::Attack);
		if (m_attackCooldownRemaining <= 0.0f)
			m_attackCooldownRemaining = m_attackCooldownDuration;
		Controller::Move(owner, glm::vec2(0.0f), dt);
		return;
	}

	SetState(State::Chase);
	FaceMovement(owner, offset, dt);
	const glm::vec3 direction = glm::normalize(offset);
	Controller::Move(owner, glm::vec2(direction.x, direction.z), dt);
}

void AIController::SetDetectionRange(float value) { if (std::isfinite(value)) m_detectionRange = std::max(0.0f, value); }
void AIController::SetAttackRange(float value) { if (std::isfinite(value)) m_attackRange = std::max(0.0f, value); }
void AIController::SetAttackCooldown(float value) { if (std::isfinite(value)) m_attackCooldownDuration = std::max(0.0f, value); }
void AIController::SetTurnSpeed(float value) { if (std::isfinite(value)) m_turnSpeed = std::max(0.0f, value); }
