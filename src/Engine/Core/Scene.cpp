#include "Engine/Core/Scene.h"
#include "Engine/Core/EventManager.h"
#include "Engine/Core/Root.h"

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

	m_entities.erase(it);
	return true;
}
