#include "Engine/Core/Scene.h"
#include "Engine/Core/EventManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/PhysicsWorld.h"

#include <iostream>
#include <algorithm>

Scene::Scene(std::string name)
	: m_name(std::move(name))
{
}

Scene::~Scene() = default;

void Scene::SetName(std::string name)
{
	m_name = std::move(name);
}

void Scene::startUp()
{
	for (const auto& entity : m_entities)
	{
		if (entity)
		{
			entity->startUp();
		}
	}
	m_firstFramePending = true;
}

void Scene::FirstFrame()
{
	if (!m_firstFramePending)
	{
		return;
	}

	m_firstFramePending = false;
	for (const auto& entity : m_entities)
	{
		if (entity)
		{
			entity->FirstFrameComponents();
		}
	}
}

void Scene::Clear()
{
	if (Root::HasCurrent())
	{
		Root::Current().Render().ClearPathedCameraTarget();
	}
	m_entities.clear();
	Root::Current().Events().Clear();
	m_firstFramePending = false;
}

Entity* Scene::AddObject(std::unique_ptr<Entity> entity)
{
	if (!entity)
	{
		return nullptr;
	}

	Entity* rawEntity = entity.get();
	m_entities.push_back(std::move(entity));
	return rawEntity;
}

LevelCollider* Scene::AddLevelCollider(std::unique_ptr<LevelCollider> collider)
{
	if (!collider)
		return nullptr;
	LevelCollider* result = collider.get();
	m_levelColliders.push_back(std::move(collider));
	return result;
}

bool Scene::RemoveLevelCollider(LevelCollider* collider)
{
	const auto iterator = std::find_if(m_levelColliders.begin(), m_levelColliders.end(),
		[collider](const std::unique_ptr<LevelCollider>& candidate) { return candidate.get() == collider; });
	if (iterator == m_levelColliders.end())
		return false;
	m_levelColliders.erase(iterator);
	return true;
}

bool Scene::RemoveObject(Entity* entity)
{
	if (!entity)
	{
		return false;
	}

	const auto it = std::find_if(m_entities.begin(), m_entities.end(), [entity](const auto& ownedEntity)
	{
		return ownedEntity.get() == entity;
	});
	if (it == m_entities.end())
	{
		return false;
	}
	if (Root::HasCurrent() &&
		Root::Current().Render().GetPathedCamera().Target() == entity)
	{
		Root::Current().Render().ClearPathedCameraTarget();
	}

	const ColliderHandle collider = PhysicsWorld::Instance().Find(*entity);
	if (collider != InvalidColliderHandle)
	{
		PhysicsWorld::Instance().Remove(collider);
	}
	m_entities.erase(it);
	return true;
}
