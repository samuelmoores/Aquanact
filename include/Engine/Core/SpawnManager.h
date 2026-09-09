#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Engine/Core/EntityStateMachine.h"

class Entity;
class Scene;
class SceneManager;

struct InstanceDefinition
{
	std::string name;
	std::string modelPath;
	std::vector<std::string> componentTypes;
	bool hasEntityStateMachineConfiguration = false;
	std::string entityStateMachineInitialState;
	std::vector<EntityStateMachine::State> entityStateMachineStates;
	std::vector<EntityStateMachine::Transition> entityStateMachineTransitions;
};

// Runtime factory and lifetime registry for entities created from instance
// definitions. Scene owns the actual Entity objects; this system tracks the
// instances it created and provides the common construction path.
class SpawnManager final
{
public:
	bool CreateInstance(InstanceDefinition definition);
	bool DeleteInstance(const std::string& definitionName);
	Entity* SpawnInstance(Scene& scene, const std::string& definitionName,
		const glm::vec3& position, const glm::vec3& rotation);
	bool Despawn(Scene& scene, Entity* entity);
	void AppendProjectState(std::string& contents, const std::filesystem::path& projectPath, const SceneManager& scenes);
	void LoadProjectState(const std::filesystem::path& projectPath);
	void ResetForProject();
	const std::unordered_map<std::string, InstanceDefinition>& Definitions() const { return m_definitions; }
	InstanceDefinition* FindDefinition(const std::string& name);
	const InstanceDefinition* FindDefinition(const std::string& name) const;
	std::size_t ActiveInstanceCount() const { return m_instances.size(); }

private:
	std::unordered_map<std::string, InstanceDefinition> m_definitions;
	std::vector<Entity*> m_instances;
};
