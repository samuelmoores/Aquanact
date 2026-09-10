#pragma once

#include "Engine/Core/Controller.h"

#include <glm/glm.hpp>

class Entity;
class EntityStateMachine;

// A reusable direct-steering controller for hostile NPCs. It acquires the
// active PlayerController, chases within detection range, and pauses in an
// attack state when it reaches the configured attack range.
class AIController final : public Controller
{
public:
	AIController() = default;

	const char* Name() const override { return "AIController"; }
	int TeamId() const override { return 2; }
	void startUp(Entity& owner) override;
	void FirstFrame(Entity& owner) override;
	void Update(Entity& owner, float dt) override;

	float DetectionRange() const { return m_detectionRange; }
	void SetDetectionRange(float value);
	float AttackRange() const { return m_attackRange; }
	void SetAttackRange(float value);
	float AttackCooldown() const { return m_attackCooldownDuration; }
	void SetAttackCooldown(float value);
	float TurnSpeed() const { return m_turnSpeed; }
	void SetTurnSpeed(float value);

	bool HasTarget() const { return m_target != nullptr; }
	bool IsChasing() const { return m_state == State::Chase; }
	bool IsAttacking() const { return m_state == State::Attack; }
	bool WasHit() const { return m_hitTrigger; }
	float TargetDistance() const { return m_targetDistance; }

	#define AI_CONTROLLER_BINDABLES(VALUE, FUNCTION) \
		FUNCTION(IsMoving) \
		FUNCTION(IsGrounded) \
		FUNCTION(HasTarget) \
		FUNCTION(IsChasing) \
		FUNCTION(IsAttacking) \
		FUNCTION(WasHit) \
		FUNCTION(TargetDistance) \
		VALUE(m_moveSpeed) \
		VALUE(m_detectionRange) \
		VALUE(m_attackRange) \
		VALUE(m_attackCooldownDuration) \
		VALUE(m_turnSpeed)
	AQUA_DECLARE_BINDABLES(AI_CONTROLLER_BINDABLES)
	#undef AI_CONTROLLER_BINDABLES

private:
	enum class State { Idle, Chase, Attack };

	void FindTarget();
	void BeginHitLock(Entity& owner);
	void SetState(State state);
	void FaceMovement(Entity& owner, const glm::vec3& direction, float dt);
	void SetAnimationState(const char* stateName);

	Entity* m_target = nullptr;
	EntityStateMachine* m_entityState = nullptr;
	State m_state = State::Idle;
	float m_detectionRange = 1200.0f;
	float m_attackRange = 150.0f;
	float m_attackCooldownDuration = 1.0f;
	float m_attackCooldownRemaining = 0.0f;
	float m_turnSpeed = 8.0f;
	float m_targetDistance = 0.0f;
	bool m_hitTrigger = false;
	bool m_pendingHit = false;
	float m_hitStopRemaining = 0.0f;
	glm::vec3 m_hitLockedPosition{0.0f};
	glm::vec3 m_hitLockedRotation{0.0f};
	bool m_hitTransformLocked = false;
};
