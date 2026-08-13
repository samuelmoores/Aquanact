#include "Engine/Core/TriggerSphere.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/PhysicsWorld.h"

#include <algorithm>
#include <cmath>

void TriggerSphere::startUp(Entity& owner)
{
	(void)owner;
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
	const std::vector<Entity*> current = PhysicsWorld::Instance().QuerySphere(
		owner.WorldCenterPosition(), m_radius, &owner);

	// Convert the query result to a set so membership checks are constant-time
	// and each entity can produce at most one enter event per update.
	std::unordered_set<Entity*> currentSet(current.begin(), current.end());

	// An entity is entering when it is present now but was absent last update.
	for (Entity* entity : currentSet)
	{
		if (!m_overlapping.contains(entity) && m_onEnter)
		{
			m_onEnter(owner, *entity);
			DispatchBindableEvent("Entered");
		}
	}
	for (Entity* entity : m_overlapping)
	{
		if (!currentSet.contains(entity) && m_onExit)
		{
			m_onExit(owner, *entity);
			DispatchBindableEvent("Exited");
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
