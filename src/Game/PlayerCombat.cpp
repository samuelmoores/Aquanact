#include "Game/PlayerCombat.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/SpawnManager.h"

#include <cmath>

void PlayerCombat::startUp(Entity&)
{
	// Attack requests are runtime-only and must not survive a session restart.
	m_attackRequested = false;
	m_projectileSpawnPending = false;
	m_attackElapsed = 0.0f;
	m_inputActions = &Root::Current().InputActions();
}

void PlayerCombat::FirstFrame(Entity& owner)
{
	m_entityState = owner.GetComponent<EntityStateMachine>();
}

void PlayerCombat::Update(Entity& owner, float dt)
{
	m_attackRequested = m_inputActions->WasPressed("Attack");
	if (m_attackRequested && !m_projectileName.empty())
	{
		m_projectileSpawnPending = true;
		m_attackElapsed = 0.0f;
	}

	if (!m_projectileSpawnPending)
		return;

	// Projectile Spawn Frame uses the same 24 fps Maya timeline convention as
	// PlayerAttack's launch frame. Frame zero preserves immediate spawning.
	m_attackElapsed += dt > 0.0f ? dt : 0.0f;
	const float spawnTime = static_cast<float>(m_projectileSpawnFrame) / 24.0f;
	if (m_projectileSpawnFrame > 0 && m_attackElapsed < spawnTime)
		return;
	m_projectileSpawnPending = false;
	if (!m_projectileName.empty())
	{
		Scene* scene = Root::Current().Scenes().ActiveLevel();
		if (scene)
		{
			glm::vec3 spawnPosition = owner.WorldPosition();
			if (!m_spawnBoneName.empty() && owner.GetMesh())
				spawnPosition = glm::vec3(owner.BuildModelMatrix()
					* glm::vec4(owner.GetMesh()->BonePosition(m_spawnBoneName), 1.0f));
			Entity* projectile = Root::Current().Spawns().SpawnInstance(*scene, m_projectileName,
				spawnPosition, owner.Rotation());
			if (projectile)
			{
				// Entity forward is local +Z; add an upward bias before applying one
				// launch impulse. Rigid-body gravity then carries it through an arc.
				const float yaw = owner.Rotation().y;
				const glm::vec3 forward(std::sin(yaw), 0.0f, std::cos(yaw));
				const glm::vec3 adjustedDirection = forward + glm::vec3(0.0f, 0.75f, 0.0f)
					+ m_launchDirection;
				const glm::vec3 launchDirection = glm::length(adjustedDirection) > 0.0001f
					? glm::normalize(adjustedDirection) : forward;
				// Physics uses centimeters and applies 981 cm/s^2 gravity. This
				// single 1200 cm/s impulse gives the projectile enough speed to arc.
				projectile->Launch(launchDirection * 1200.0f);
				projectile->SetLinearDamping(1.25f);
				PhysicsWorld::Instance().Update(*projectile);
			}
		}
	}
}

// Keep component metadata in the header with the binding/event list macros.
// Add runtime logic here only if the component needs it.
