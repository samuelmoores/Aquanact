#include "Game/PlayerAttack.h"
#include "Game/AttackSphere.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/SpawnManager.h"

#include <utility>

namespace
{
	bool HasReachedLaunchFrame(const PlayerAttack& attack, float attackElapsed)
	{
		constexpr float mayaFramesPerSecond = 24.0f;
		if (attack.AttackLaunchFrame() <= 0)
			return true;

		// The editor value is a Maya timeline frame, while Assimp's animation
		// ticks can use another rate. Measure from the attack input itself so the
		// previous state-machine state's elapsed time cannot trigger an immediate
		// launch before the sphere has played.
		const float launchTimeSeconds = static_cast<float>(attack.AttackLaunchFrame())
			/ mayaFramesPerSecond;
		return attackElapsed >= launchTimeSeconds;
	}

}

glm::vec3 PlayerAttack::AttackInstanceSpawnPosition(Entity& owner) const
{
	const glm::vec3 bonePosition = !m_attackInstanceBoneName.empty() && owner.GetMesh()
		? owner.GetMesh()->BonePosition(m_attackInstanceBoneName) : glm::vec3(0.0f);
	return bonePosition + m_attackInstanceOffset;
}

void PlayerAttack::startUp(Entity& owner)
{
	m_inputActions = &Root::Current().InputActions();
	m_attackTrigger = 0.0f;
	m_attackInstance = nullptr;
	m_attackSequenceElapsed = 0.0f;
	m_attackSequenceActive = false;
}

void PlayerAttack::Update(Entity& owner, float dt)
{
	if (!m_inputActions)
	{
		m_inputActions = &Root::Current().InputActions();
	}
	m_attackTrigger = m_inputActions->WasPressed("Attack") ? 1.0f : 0.0f;
	bool spawnedAttackInstance = false;
	if (m_attackTrigger > 0.5f)
	{
		if (!m_attackInstance && !m_attackInstanceName.empty() && Root::Current().Scenes().ActiveLevel())
		{
			Scene* scene = Root::Current().Scenes().ActiveLevel();
			m_attackInstance = Root::Current().Spawns().SpawnInstance(
				*scene, m_attackInstanceName,
				AttackInstanceSpawnPosition(owner), glm::vec3(0.0f), &owner);
			if (m_attackInstance)
			{
				spawnedAttackInstance = true;
				m_attackSequenceElapsed = 0.0f;
				m_attackSequenceActive = true;
				if (EntityStateMachine* sphereState = m_attackInstance->GetEntityState())
					sphereState->SetCurrentStateLooping(false);
			}
		}
	}

	if (m_attackSequenceActive && !spawnedAttackInstance)
		m_attackSequenceElapsed += dt > 0.0f ? dt : 0.0f;

	// The instance remains attached to the owner and follows the selected bone.
	// Launching, state-machine transitions, and projectile movement are deferred
	// until PlayerAttack has a larger gameplay scope.
	if (m_attackInstance)
	{
		AttackSphere* attackSphere = m_attackInstance->GetComponent<AttackSphere>();
		if (!attackSphere)
			attackSphere = m_attackInstance->AddComponent<AttackSphere>();
		if (attackSphere && !attackSphere->Launched())
		{
			const glm::vec3 spawnPosition = AttackInstanceSpawnPosition(owner);
			const bool launchFrameReached = HasReachedLaunchFrame(*this, m_attackSequenceElapsed);
			if (launchFrameReached)
			{
				Entity* target = nullptr;
				if (!m_attackTargetInstanceName.empty())
				{
					target = Root::Current().Spawns().FindActiveInstance(
						*Root::Current().Scenes().ActiveLevel(), m_attackTargetInstanceName);
				}
				// An explicitly selected instance must resolve to an active object;
				// do not silently fall back to the closest target.
				if (m_attackTargetInstanceName.empty() || target)
				{
					if (attackSphere->Launch(*Root::Current().Scenes().ActiveLevel(), &owner,
						m_attackInstanceSpeed, target, m_attackTargetBoneName,
						m_attackTargetInstanceName))
						m_attackSequenceActive = false;
				}
				else
				{
					// Keep the projectile at its authored launch point while the
					// selected instance is respawning. Leaving its state-machine
					// animation running here makes the sphere visibly loop and can
					// move it away from the point from which it will eventually launch.
					if (EntityStateMachine* stateMachine = m_attackInstance->GetEntityState())
					{
						stateMachine->SetCurrentStateLooping(false);
						stateMachine->SetTransformAnimationEnabled(false);
					}
				}
			}
			else
			{
				m_attackInstance->Translate(spawnPosition - m_attackInstance->Position());
				if (EntityStateMachine* stateMachine = m_attackInstance->GetEntityState())
					stateMachine->SetTransformAnimationOffset(spawnPosition);
			}
		}
		PhysicsWorld::Instance().Update(*m_attackInstance);
	}
}
