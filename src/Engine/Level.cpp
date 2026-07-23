#include "Engine/Level.h"

Level::Level(std::string name)
	: m_name(std::move(name))
{
}

Level::~Level() = default;

void Level::SetName(std::string name)
{
	m_name = std::move(name);
}

void Level::Clear()
{
	m_objects.clear();
}

Entity* Level::AddObject(std::unique_ptr<Entity> object)
{
	if (!object)
	{
		return nullptr;
	}

	Entity* rawObject = object.get();
	m_objects.push_back(std::move(object));
	return rawObject;
}
