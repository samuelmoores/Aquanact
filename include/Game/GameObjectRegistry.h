#pragma once

#include "Game/GameObject.h"

#include <string>
#include <vector>

class GameObjectRegistry
{
public:
	void Clear();

	void Register(GameObject* object);
	void Unregister(GameObject* object);

	GameObject* FindByName(const std::string& name) const;
	const std::vector<GameObject*>& Objects() const { return m_objects; }

private:
	std::vector<GameObject*> m_objects;
};
