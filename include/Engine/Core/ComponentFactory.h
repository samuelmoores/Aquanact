#pragma once

#include "Engine/Core/Component.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class Entity;

class ComponentFactory
{
public:
	using Creator = std::function<std::unique_ptr<Component>(Entity&)>;

	static ComponentFactory& Instance();

	void Register(std::string typeName, Creator creator);
	bool Unregister(const std::string& typeName);

	std::unique_ptr<Component> Create(const std::string& typeName, Entity& owner) const;
	std::vector<std::string> Names() const;

private:
	std::unordered_map<std::string, Creator> m_creators;
};
