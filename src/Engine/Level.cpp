#include "Engine/Level.h"
#include "Engine/EventManager.h"
#include "Engine/Root.h"

#include <iostream>
#include <algorithm>

Level::Level(std::string name)
	: m_name(std::move(name))
{
}

Level::~Level() = default;

void Level::SetName(std::string name)
{
	m_name = std::move(name);
}

void Level::startUp()
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

void Level::FirstFrame()
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

void Level::Clear()
{
	m_entities.clear();
	Root::Current().Events().Clear();
	m_firstFramePending = false;
}

Entity* Level::AddObject(std::unique_ptr<Entity> entity)
{
	if (!entity)
	{
		return nullptr;
	}

	Entity* rawEntity = entity.get();
	m_entities.push_back(std::move(entity));
	return rawEntity;
}

bool Level::RemoveObject(Entity* entity)
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
