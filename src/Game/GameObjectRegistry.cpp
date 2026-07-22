#include "Game/GameObjectRegistry.h"

#include <algorithm>

void GameObjectRegistry::Clear()
{
	m_objects.clear();
}

void GameObjectRegistry::Register(GameObject* object)
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

void GameObjectRegistry::Unregister(GameObject* object)
{
	const auto it = std::remove(m_objects.begin(), m_objects.end(), object);
	m_objects.erase(it, m_objects.end());
}

GameObject* GameObjectRegistry::FindByName(const std::string& name) const
{
	for (GameObject* object : m_objects)
	{
		if (object && object->Name() == name)
		{
			return object;
		}
	}
	return nullptr;
}
