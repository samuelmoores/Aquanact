#pragma once

#include "Engine/Core/Entity.h"

#include <string>
#include <vector>

class EntityRegistry
{
public:
	void Clear();

	void Register(Entity* object);
	void Unregister(Entity* object);

	Entity* FindByName(const std::string& name) const;
	const std::vector<Entity*>& Objects() const { return m_objects; }

private:
	std::vector<Entity*> m_objects;
};

