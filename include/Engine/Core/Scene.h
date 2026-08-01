#pragma once

#include "Engine/Core/Entity.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class Scene
{
public:
	explicit Scene(std::string name = "Scene");
	~Scene();

	const std::string& Name() const { return m_name; }
	void SetName(std::string name);

	void startUp();
	void FirstFrame();
	void Clear();
	Entity* AddObject(std::unique_ptr<Entity> entity);
	bool RemoveObject(Entity* entity);
	const std::vector<std::unique_ptr<Entity>>& Entities() const { return m_entities; }
	const std::vector<std::unique_ptr<Entity>>& Objects() const { return m_entities; }

private:
	std::string m_name;
	std::vector<std::unique_ptr<Entity>> m_entities;
	bool m_firstFramePending = false;
};



