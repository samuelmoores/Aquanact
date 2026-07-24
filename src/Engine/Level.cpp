#include "Engine/Level.h"
#include "Engine/EventManager.h"
#include "Engine/Globals.h"

#include <iostream>

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
	gEventManager.Clear();
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
	gEventManager.Clear();
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
