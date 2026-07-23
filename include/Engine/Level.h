#pragma once

#include "Engine/Entity.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class Level
{
public:
	explicit Level(std::string name = "Level");
	~Level();

	const std::string& Name() const { return m_name; }
	void SetName(std::string name);

	void Clear();
	Entity* AddObject(std::unique_ptr<Entity> object);
	const std::vector<std::unique_ptr<Entity>>& Objects() const { return m_objects; }

private:
	std::string m_name;
	std::vector<std::unique_ptr<Entity>> m_objects;
};
