#include "Game/PlayerAttack.h"
#include "Game/AttackSphere.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/SpawnManager.h"

#include <algorithm>
#include <filesystem>
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

	void EnsureAttackInstanceDefinition(const std::string& definitionName)
	{
		if (definitionName.empty())
			return;

		SpawnManager& spawns = Root::Current().Spawns();
		InstanceDefinition* definition = spawns.FindDefinition(definitionName);
		if (!definition)
		{
			const std::filesystem::path modelPath = Root::Current().Projects().ProjectAssetsDirectory()
				/ "models" / (definitionName + ".fbx");
			if (!std::filesystem::is_regular_file(modelPath))
				return;

			InstanceDefinition recovered;
			recovered.name = definitionName;
			recovered.modelPath = modelPath.string();
			spawns.CreateInstance(std::move(recovered));
			definition = spawns.FindDefinition(definitionName);
		}
		if (!definition)
			return;

		definition->blocksCollision = false;
		definition->ignoreCameraCollision = true;
		definition->blocksCameraView = false;
		if (std::find(definition->componentTypes.begin(), definition->componentTypes.end(), "EntityStateMachine")
			== definition->componentTypes.end())
		{
			definition->componentTypes.push_back("EntityStateMachine");
		}

		auto conjureState = std::find_if(
			definition->entityStateMachineStates.begin(), definition->entityStateMachineStates.end(),
			[](const EntityStateMachine::State& state) { return state.name == "conjure"; });
		if (conjureState == definition->entityStateMachineStates.end())
		{
			EntityStateMachine::State state;
			state.name = "conjure";
			state.animationName = "sphere_conjure.fbx";
			state.loop = false;
			state.useTransformAnimation = true;
			state.transformAnimationName = "sphere_conjure.fbx";
			definition->entityStateMachineStates.push_back(std::move(state));
		}
		else
		{
			conjureState->animationName = "sphere_conjure.fbx";
			conjureState->loop = false;
			conjureState->useTransformAnimation = true;
			conjureState->transformAnimationName = "sphere_conjure.fbx";
		}
		definition->hasEntityStateMachineConfiguration = true;
		definition->entityStateMachineInitialState = "conjure";
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
	m_attackUsed = false;
}

void PlayerAttack::Update(Entity& owner, float dt)
{
	if (!m_inputActions)
	{
		m_inputActions = &Root::Current().InputActions();
	}
	m_attackTrigger = m_inputActions->WasPressed("Attack") && !m_attackUsed ? 1.0f : 0.0f;
	bool spawnedAttackInstance = false;
	if (m_attackTrigger > 0.5f)
	{
		m_attackUsed = true;
		// Keep the authored transition condition as a bindable diagnostic, but
		// explicitly request the attack state so a one-frame key press cannot be
		// missed by the state-machine update order.
		if (EntityStateMachine* playerState = owner.GetEntityState())
			playerState->SetDesiredState("attack");
		if (!m_attackInstance && !m_attackInstanceName.empty() && Root::Current().Scenes().ActiveLevel())
		{
			EnsureAttackInstanceDefinition(m_attackInstanceName);
			Scene* scene = Root::Current().Scenes().ActiveLevel();
			m_attackInstance = Root::Current().Spawns().SpawnInstance(
				*scene, m_attackInstanceName,
				AttackInstanceSpawnPosition(owner), glm::vec3(0.0f), &owner);
			if (m_attackInstance)
			{
				spawnedAttackInstance = true;
				m_attackSequenceElapsed = 0.0f;
				m_attackSequenceActive = true;
				m_attackInstance->SetBlocksCollision(false);
				m_attackInstance->SetIgnoreCameraCollision(true);
				m_attackInstance->SetBlocksCameraView(false);
				if (EntityStateMachine* sphereState = m_attackInstance->GetEntityState())
				{
					sphereState->SetTransformAnimationEnabled(true);
					sphereState->SetTransformAnimationOffset(AttackInstanceSpawnPosition(owner));
					sphereState->SetInitialState("conjure");
					sphereState->SetCurrentStateLooping(false);
				}
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
