#include "Engine/Core/ComponentFactory.h"

#include "Engine/Core/Entity.h"

ComponentFactory& ComponentFactory::Instance()
{
	static ComponentFactory instance;
	return instance;
}

void ComponentFactory::Register(std::string typeName, Creator creator)
{
	m_creators[std::move(typeName)] = std::move(creator);
}

bool ComponentFactory::Unregister(const std::string& typeName)
{
	return m_creators.erase(typeName) > 0;
}

std::unique_ptr<Component> ComponentFactory::Create(const std::string& typeName, Entity& owner) const
{
	const auto it = m_creators.find(typeName);
	if (it == m_creators.end())
	{
		return nullptr;
	}

	std::unique_ptr<Component> component = it->second(owner);
	if (component)
	{
		component->SetOwner(&owner);
	}
	return component;
}

std::vector<std::string> ComponentFactory::Names() const
{
	std::vector<std::string> names;
	names.reserve(m_creators.size());
	for (const auto& entry : m_creators)
	{
		names.push_back(entry.first);
	}
	return names;
}
