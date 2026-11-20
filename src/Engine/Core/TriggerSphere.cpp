#include "Engine/Core/TriggerSphere.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/PhysicsWorld.h"

#include <algorithm>
#include <cmath>

void TriggerSphere::startUp(Entity& owner)
{
	(void)owner;
}

void TriggerSphere::OnEntityEnter(Entity& triggerOwner, Entity& enteringEntity)
{
	(void)triggerOwner;
	(void)enteringEntity;
	// Add shared trigger-entry gameplay here, or override this hook in a subclass.
}

void TriggerSphere::OnEntityExit(Entity& triggerOwner, Entity& leavingEntity)
{
	(void)triggerOwner;
	(void)leavingEntity;
	// Add shared trigger-exit gameplay here, or override this hook in a subclass.
}

glm::vec3 TriggerSphere::WorldPosition(Entity& owner) const
{
	const glm::vec3 worldOffset = glm::vec3(owner.BuildModelMatrix()
		* glm::vec4(m_positionOffset, 0.0f));
	return owner.WorldCenterPosition() + worldOffset;
}

void TriggerSphere::Update(Entity& owner, float deltaTime)
{
	// A disabled trigger should not retain stale occupants. If it is enabled
	// again later, entities already inside must be treated as fresh entries.
	if (!m_enabled)
	{
		m_overlapping.clear();
		return;
	}

	// Query the physics world rather than duplicating collider overlap math here.
	// The owner is excluded so a trigger never enters itself.
	std::vector<Entity*> current = PhysicsWorld::Instance().QuerySphere(
		WorldPosition(owner), m_radius, &owner);
	if (owner.IsSubmeshComponentOwner() && owner.Parent())
		current.erase(std::remove(current.begin(), current.end(), owner.Parent()), current.end());

	// Convert the query result to a set so membership checks are constant-time
	// and each entity can produce at most one enter callback per update.
	std::unordered_set<Entity*> currentSet(current.begin(), current.end());

	// An entity is entering when it is present now but was absent last update.
	for (Entity* entity : currentSet)
	{
		if (!m_overlapping.contains(entity))
		{
			OnEntityEnter(owner, *entity);
			entity->OnTriggerEnter(owner);
		}
	}
	for (Entity* entity : m_overlapping)
	{
		if (!currentSet.contains(entity))
		{
			OnEntityExit(owner, *entity);
			entity->OnTriggerExit(owner);
		}
	}

	// Replace the previous frame's occupants so exits are naturally forgotten
	// and a later re-entry can fire the callback again.
	m_overlapping = std::move(currentSet);
	(void)deltaTime;
}

void TriggerSphere::SetRadius(float radius)
{
	if (std::isfinite(radius))
	{
		m_radius = std::max(radius, 0.0f);
	}
}
