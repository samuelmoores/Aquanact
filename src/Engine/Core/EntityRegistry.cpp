#include "Engine/Core/EntityRegistry.h"

#include <algorithm>

void EntityRegistry::Clear()
{
	m_objects.clear();
}

void EntityRegistry::Register(Entity* object)
{
	if (!object)
	{
		return;
	}

	if (std::find(m_objects.begin(), m_objects.end(), object) == m_objects.end())
	{
		m_objects.push_back(object);
	}
}

void EntityRegistry::Unregister(Entity* object)
{
	const auto it = std::remove(m_objects.begin(), m_objects.end(), object);
	m_objects.erase(it, m_objects.end());
}

Entity* EntityRegistry::FindByName(const std::string& name) const
{
	for (Entity* object : m_objects)
	{
		if (object && object->Name() == name)
		{
			return object;
		}
	}
	return nullptr;
}
