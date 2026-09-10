#include "Engine/Core/SpawnManager.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectStateSerializer.h"
#include "Engine/Core/PhysicsWorld.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace
{
	std::string PortableModelPath(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		std::transform(path.begin(), path.end(), path.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		const std::size_t assetsPosition = path.find("assets/");
		return assetsPosition == std::string::npos
			? path : path.substr(assetsPosition + std::string("assets/").size());
	}

	void AppendOperand(std::string& contents, const EntityStateMachine::Operand& operand)
	{
		contents += ";" + std::to_string(static_cast<int>(operand.type)) + ";" + std::to_string(operand.constantValue);
		contents += ";" + ProjectStateSerializer::EscapeField(operand.componentName);
		contents += ";" + ProjectStateSerializer::EscapeField(operand.memberName);
	}

	void AppendCondition(std::string& contents, const EntityStateMachine::Condition& condition)
	{
		AppendOperand(contents, condition.left);
		contents += ";" + std::to_string(static_cast<int>(condition.comparator));
		AppendOperand(contents, condition.right);
	}

	bool ReadOperand(const std::vector<std::string>& fields, std::size_t& index, EntityStateMachine::Operand& operand)
	{
		if (index + 3 >= fields.size()) return false;
		operand.type = static_cast<EntityStateMachine::OperandType>(std::stoi(fields[index++]));
		operand.constantValue = std::stof(fields[index++]);
		operand.componentName = ProjectStateSerializer::UnescapeField(fields[index++]);
		operand.memberName = ProjectStateSerializer::UnescapeField(fields[index++]);
		return true;
	}

	bool ReadCondition(const std::vector<std::string>& fields, std::size_t& index, EntityStateMachine::Condition& condition)
	{
		if (!ReadOperand(fields, index, condition.left) || index >= fields.size()) return false;
		condition.comparator = static_cast<EntityStateMachine::Comparator>(std::stoi(fields[index++]));
		return ReadOperand(fields, index, condition.right);
	}

	void RecoverDefinitionFromEntity(InstanceDefinition& definition, const Entity& entity)
	{
	definition.name = entity.Name();
	definition.modelPath = entity.SourcePath();
	definition.blocksCollision = entity.BlocksCollision();
	definition.ignoreCameraCollision = entity.IgnoreCameraCollision();
	definition.blocksCameraView = entity.BlocksCameraView();
	definition.showPhysicsBoundingBox = entity.ShowPhysicsBoundingBox();
	definition.physicsColliderShape = static_cast<int>(entity.GetPhysicsColliderShape());
	if (entity.Parent())
		definition.attachToEntityName = entity.Parent()->Name();
		for (const Component* component : entity.Components())
		{
			if (!component) continue;
			definition.componentTypes.push_back(component->Name());
			if (const auto* machine = dynamic_cast<const EntityStateMachine*>(component))
			{
				definition.hasEntityStateMachineConfiguration = true;
				definition.entityStateMachineInitialState = machine->InitialState();
				definition.entityStateMachineStates = machine->States();
				definition.entityStateMachineTransitions = machine->Transitions();
			}
		}
	}
}

bool SpawnManager::CreateInstance(InstanceDefinition definition)
{
	if (definition.name.empty() || definition.modelPath.empty()) return false;
	m_definitions[definition.name] = std::move(definition);
	return true;
}

InstanceDefinition* SpawnManager::FindDefinition(const std::string& name)
{
	const auto found = m_definitions.find(name);
	return found == m_definitions.end() ? nullptr : &found->second;
}

const InstanceDefinition* SpawnManager::FindDefinition(const std::string& name) const
{
	const auto found = m_definitions.find(name);
	return found == m_definitions.end() ? nullptr : &found->second;
}

Entity* SpawnManager::FindActiveInstance(const Scene& scene, const std::string& definitionName) const
{
	if (definitionName.empty()) return nullptr;
	for (Entity* entity : m_instances)
	{
		if (!entity || entity->Name() != definitionName)
			continue;
		for (const auto& object : scene.Objects())
			if (object.get() == entity)
				return entity;
	}

	// A scene can contain an instance loaded from project data before the
	// runtime registry has seen it, or a restored replacement may have been
	// created by another scene path. The name is the serialized instance
	// identity, so accept a live scene object when it matches a known
	// definition as well.
	const InstanceDefinition* definition = FindDefinition(definitionName);
	if (!definition) return nullptr;
	for (const auto& object : scene.Objects())
		if (object && (object->Name() == definitionName
			|| PortableModelPath(object->SourcePath()) == PortableModelPath(definition->modelPath)))
			return object.get();
	return nullptr;
}

bool SpawnManager::DeleteInstance(const std::string& definitionName)
{
	return m_definitions.erase(definitionName) != 0;
}

void SpawnManager::ResetForProject()
{
	m_definitions.clear();
	m_instances.clear();
}

void SpawnManager::AppendProjectState(std::string& contents, const std::filesystem::path& projectPath, const SceneManager& scenes)
{
	std::unordered_set<Entity*> liveEntities;
	for (const auto& scene : scenes.Levels())
	{
		if (!scene) continue;
		for (const auto& entity : scene->Objects())
			if (entity) liveEntities.insert(entity.get());
	}
	m_instances.erase(std::remove_if(m_instances.begin(), m_instances.end(),
		[&liveEntities](Entity* entity)
		{
			return !entity || !liveEntities.contains(entity);
		}), m_instances.end());

	for (Entity* entity : m_instances)
	{
		if (!entity || m_definitions.find(entity->Name()) != m_definitions.end()) continue;
		InstanceDefinition recovered;
		RecoverDefinitionFromEntity(recovered, *entity);
		if (!recovered.name.empty() && !recovered.modelPath.empty())
			m_definitions[recovered.name] = std::move(recovered);
	}
	for (const auto& scene : scenes.Levels())
	{
		if (!scene) continue;
		for (const auto& entity : scene->Objects())
		{
			if (!entity || m_definitions.find(entity->Name()) != m_definitions.end()
				|| !entity->GetComponentByName("AIController") || !entity->GetEntityState())
				continue;
			InstanceDefinition recovered;
			RecoverDefinitionFromEntity(recovered, *entity);
			if (!recovered.name.empty() && !recovered.modelPath.empty())
				m_definitions[recovered.name] = std::move(recovered);
		}
	}

	for (const auto& entry : m_definitions)
	{
		const InstanceDefinition& definition = entry.second;
	contents += "instance;" + ProjectStateSerializer::EscapeField(definition.name);
		contents += ";" + ProjectStateSerializer::EscapeField(
			ProjectStateSerializer::MakePortableSourcePath(projectPath, definition.modelPath).string());
		contents += ";attach;" + ProjectStateSerializer::EscapeField(definition.attachToEntityName);
		contents += ";" + std::to_string(definition.componentTypes.size());
		for (const std::string& type : definition.componentTypes)
			contents += ";" + ProjectStateSerializer::EscapeField(type);
		contents += ";" + std::to_string(definition.hasEntityStateMachineConfiguration ? 1 : 0);
		contents += ";" + ProjectStateSerializer::EscapeField(definition.entityStateMachineInitialState);
		contents += ";" + std::to_string(definition.entityStateMachineStates.size());
		for (const auto& state : definition.entityStateMachineStates)
		{
			contents += ";" + ProjectStateSerializer::EscapeField(state.name);
			contents += ";" + ProjectStateSerializer::EscapeField(
				ProjectStateSerializer::MakePortableSourcePath(projectPath, state.animationName).string());
			contents += ";" + std::to_string(state.blocksMovement ? 1 : 0);
			contents += ";" + std::to_string(state.blocksInput ? 1 : 0);
			contents += ";sequence2;" + std::to_string(state.useAnimationSequence ? state.animationSequence.size() : 0);
			if (state.useAnimationSequence)
				for (const std::string& animation : state.animationSequence)
					contents += ";" + ProjectStateSerializer::MakePortableSourcePath(projectPath, animation).string();
			contents += ";waitforcompletion;" + std::to_string(state.waitForCompletion ? 1 : 0);
			contents += ";loop;" + std::to_string(state.loop ? 1 : 0);
			contents += ";transform2;" + std::to_string(state.useTransformAnimation ? 1 : 0);
			for (const glm::vec3& value : { state.transformStartPosition, state.transformEndPosition, state.transformStartRotation, state.transformEndRotation, state.transformStartScale, state.transformEndScale })
				contents += ";" + std::to_string(value.x) + ";" + std::to_string(value.y) + ";" + std::to_string(value.z);
		contents += ";" + std::to_string(state.transformDuration);
		contents += ";transformclip;" + ProjectStateSerializer::EscapeField(
			ProjectStateSerializer::MakePortableSourcePath(projectPath, state.transformAnimationName).string());
			contents += ";" + std::to_string(state.soundEvents.size());
			for (const auto& event : state.soundEvents)
			{
				contents += ";" + ProjectStateSerializer::EscapeField(event.soundName);
				contents += ";" + std::to_string(event.frame) + ";" + std::to_string(event.volume);
				contents += ";" + std::to_string(event.randomSample ? 1 : 0);
			}
		}
		contents += ";" + std::to_string(definition.entityStateMachineTransitions.size());
		for (const auto& transition : definition.entityStateMachineTransitions)
		{
			contents += ";" + ProjectStateSerializer::EscapeField(transition.from);
			contents += ";" + ProjectStateSerializer::EscapeField(transition.to);
			contents += ";" + std::to_string(transition.blendSeconds);
		contents += ";0";
			const auto& conditions = transition.conditions.empty()
				? std::vector<EntityStateMachine::Condition>{ transition.condition } : transition.conditions;
			contents += ";" + std::to_string(conditions.size());
			for (const auto& condition : conditions) AppendCondition(contents, condition);
		}
		contents += ";physics;" + std::to_string(definition.blocksCollision ? 1 : 0);
		contents += ";" + std::to_string(definition.ignoreCameraCollision ? 1 : 0);
		contents += ";" + std::to_string(definition.blocksCameraView ? 1 : 0);
		contents += ";" + std::to_string(definition.showPhysicsBoundingBox ? 1 : 0);
		contents += ";" + std::to_string(definition.physicsColliderShape) + "\n";
	}
}

void SpawnManager::LoadProjectState(const std::filesystem::path& projectPath)
{
	std::ifstream file(projectPath);
	if (!file) return;
	std::string line;
	while (std::getline(file, line))
	{
		const std::vector<std::string> fields = ProjectStateSerializer::SplitFields(line);
		if (fields.empty() || fields[0] != "instance") continue;
		try
		{
			std::size_t index = 1;
			InstanceDefinition definition;
			definition.name = ProjectStateSerializer::UnescapeField(fields.at(index++));
			definition.modelPath = ProjectStateSerializer::ResolveSourcePath(projectPath, fields.at(index++)).string();
			// The attach field is optional so projects written before attachment
			// support remain loadable.
			if (index < fields.size() && fields[index] == "attach")
			{
				++index;
				definition.attachToEntityName = ProjectStateSerializer::UnescapeField(fields.at(index++));
			}
			const int componentCount = std::stoi(fields.at(index++));
			for (int i = 0; i < componentCount; ++i)
				definition.componentTypes.push_back(ProjectStateSerializer::UnescapeField(fields.at(index++)));
			definition.hasEntityStateMachineConfiguration = std::stoi(fields.at(index++)) != 0;
			definition.entityStateMachineInitialState = ProjectStateSerializer::UnescapeField(fields.at(index++));
			const int stateCount = std::stoi(fields.at(index++));
			for (int i = 0; i < stateCount; ++i)
			{
				EntityStateMachine::State state;
				state.name = ProjectStateSerializer::UnescapeField(fields.at(index++));
				state.animationName = ProjectStateSerializer::ResolveSourcePath(
					projectPath, ProjectStateSerializer::UnescapeField(fields.at(index++))).string();
				state.blocksMovement = std::stoi(fields.at(index++)) != 0;
				state.blocksInput = std::stoi(fields.at(index++)) != 0;
				if (index < fields.size() && fields[index] == "sequence2")
				{
					++index;
					const int sequenceCount = std::stoi(fields.at(index++));
					state.useAnimationSequence = sequenceCount > 0;
					for (int sequenceIndex = 0; sequenceIndex < sequenceCount; ++sequenceIndex)
						state.animationSequence.push_back(ProjectStateSerializer::ResolveSourcePath(
							projectPath, fields.at(index++)).string());
				}
				if (index < fields.size() && fields[index] == "waitforcompletion")
				{
					++index;
					state.waitForCompletion = std::stoi(fields.at(index++)) != 0;
				}
				if (index < fields.size() && fields[index] == "loop")
				{
					++index;
					state.loop = std::stoi(fields.at(index++)) != 0;
				}
				if (index < fields.size() && fields[index] == "transform2")
				{
					++index;
					state.useTransformAnimation = std::stoi(fields.at(index++)) != 0;
					glm::vec3* values[] = { &state.transformStartPosition, &state.transformEndPosition, &state.transformStartRotation, &state.transformEndRotation, &state.transformStartScale, &state.transformEndScale };
					for (glm::vec3* value : values) { value->x = std::stof(fields.at(index++)); value->y = std::stof(fields.at(index++)); value->z = std::stof(fields.at(index++)); }
					state.transformDuration = std::stof(fields.at(index++));
					if (index < fields.size() && fields[index] == "transformclip")
					{
						++index;
						state.transformAnimationName = fields.at(index++);
					}
				}
				const int soundCount = std::stoi(fields.at(index++));
				for (int soundIndex = 0; soundIndex < soundCount; ++soundIndex)
				{
					EntityStateMachine::SoundEvent event;
					event.soundName = ProjectStateSerializer::UnescapeField(fields.at(index++));
					event.frame = std::stof(fields.at(index++));
					event.volume = std::stof(fields.at(index++));
					event.randomSample = std::stoi(fields.at(index++)) != 0;
					state.soundEvents.push_back(std::move(event));
				}
				definition.entityStateMachineStates.push_back(std::move(state));
			}
			const int transitionCount = std::stoi(fields.at(index++));
			for (int i = 0; i < transitionCount; ++i)
			{
				EntityStateMachine::Transition transition;
				transition.from = ProjectStateSerializer::UnescapeField(fields.at(index++));
				transition.to = ProjectStateSerializer::UnescapeField(fields.at(index++));
				transition.blendSeconds = std::stof(fields.at(index++));
				transition.waitForCurrentStateComplete = std::stoi(fields.at(index++)) != 0;
				const int conditionCount = std::stoi(fields.at(index++));
				for (int conditionIndex = 0; conditionIndex < conditionCount; ++conditionIndex)
				{
					EntityStateMachine::Condition condition;
					if (!ReadCondition(fields, index, condition)) throw std::runtime_error("invalid instance condition");
					transition.conditions.push_back(std::move(condition));
				}
				if (!transition.conditions.empty()) transition.condition = transition.conditions.front();
				definition.entityStateMachineTransitions.push_back(std::move(transition));
			}
			if (index < fields.size() && fields[index] == "physics")
			{
				++index;
				definition.blocksCollision = std::stoi(fields.at(index++)) != 0;
				definition.ignoreCameraCollision = std::stoi(fields.at(index++)) != 0;
				definition.blocksCameraView = std::stoi(fields.at(index++)) != 0;
				definition.showPhysicsBoundingBox = std::stoi(fields.at(index++)) != 0;
				definition.physicsColliderShape = std::stoi(fields.at(index++));
			}
			if (!definition.name.empty() && !definition.modelPath.empty())
				m_definitions[definition.name] = std::move(definition);
		}
		catch (...)
		{
			// Optional instance records must not make an otherwise valid project unloadable.
		}
	}

	const auto canonicalSalvador = m_definitions.find("DrSalvador");
	const auto redundantSalvador = m_definitions.find("drsalvador.fbx");
	if (canonicalSalvador != m_definitions.end() && redundantSalvador != m_definitions.end()
		&& PortableModelPath(canonicalSalvador->second.modelPath)
			== PortableModelPath(redundantSalvador->second.modelPath))
	{
		m_definitions.erase(redundantSalvador);
	}
}

Entity* SpawnManager::SpawnInstance(Scene& scene, const std::string& definitionName,
	const glm::vec3& position, const glm::vec3& rotation, Entity* parent)
{
	const auto definition = m_definitions.find(definitionName);
	if (definition == m_definitions.end()) return nullptr;
	if (!parent && !definition->second.attachToEntityName.empty())
		parent = scene.FindObjectByName(definition->second.attachToEntityName);

	auto instance = std::make_unique<Entity>(definition->second.modelPath.c_str(), false);
	if (!instance->GetMesh())
		return nullptr;

	instance->SetName(definition->second.name);
	instance->SetBlocksCollision(definition->second.blocksCollision);
	instance->SetIgnoreCameraCollision(definition->second.ignoreCameraCollision);
	instance->SetBlocksCameraView(definition->second.blocksCameraView);
	instance->SetShowPhysicsBoundingBox(definition->second.showPhysicsBoundingBox);
	instance->SetPhysicsColliderShape(definition->second.physicsColliderShape == 1
		? PhysicsColliderShape::Capsule
		: definition->second.physicsColliderShape == 2 ? PhysicsColliderShape::Convex : PhysicsColliderShape::Box);
	if (parent)
	{
		// When a parent is supplied, the spawn transform is local to that parent.
		// Attach before physics registration so the initial collider uses the
		// child's inherited world transform.
		if (!instance->AttachTo(*parent, position, rotation))
			return nullptr;
	}
	else
	{
		instance->Translate(position);
		instance->SetRotation(rotation);
	}
	for (const std::string& componentType : definition->second.componentTypes)
	{
		if (instance->GetComponentByName(componentType))
			continue;
		if (std::unique_ptr<Component> component = ComponentFactory::Instance().Create(componentType, *instance))
			instance->AddComponent(std::move(component));
	}
	if (EntityStateMachine* stateMachine = instance->GetEntityState())
	{
		const InstanceDefinition& source = definition->second;
		if (source.hasEntityStateMachineConfiguration)
		{
			for (const EntityStateMachine::State& state : source.entityStateMachineStates)
			{
				const bool waitForCompletion = state.waitForCompletion || std::any_of(
					source.entityStateMachineTransitions.begin(), source.entityStateMachineTransitions.end(),
					[&state](const auto& transition)
					{
						return transition.from == state.name && transition.waitForCurrentStateComplete;
					});
				std::string animationName = state.animationName;
				int animationIndex = stateMachine->FindAnimationIndex(animationName);
				if (animationIndex < 0)
					animationIndex = stateMachine->FindAnimationIndex(state.name);
				if (animationIndex >= 0)
					animationName = stateMachine->AnimationNames()[static_cast<std::size_t>(animationIndex)];
				std::vector<std::string> animationSequence;
				for (const std::string& animation : state.animationSequence)
				{
					const int sequenceIndex = stateMachine->FindAnimationIndex(animation);
					animationSequence.push_back(sequenceIndex >= 0
						? stateMachine->AnimationNames()[static_cast<std::size_t>(sequenceIndex)] : animation);
				}
				if (!stateMachine->AddState(state.name, std::move(animationName), state.blocksMovement, state.blocksInput,
					state.useAnimationSequence, std::move(animationSequence), waitForCompletion, state.loop))
					continue;
				if (!state.transformAnimationName.empty())
					stateMachine->SetStateTransform(stateMachine->States().size() - 1, state.transformAnimationName);
				else
					stateMachine->SetStateTransform(stateMachine->States().size() - 1, state.useTransformAnimation,
						state.transformStartPosition, state.transformEndPosition, state.transformStartRotation,
						state.transformEndRotation, state.transformStartScale, state.transformEndScale,
						state.transformDuration);
				for (const EntityStateMachine::SoundEvent& event : state.soundEvents)
					stateMachine->AddStateSoundEvent(state.name, event);
			}
			stateMachine->SetInitialState(source.entityStateMachineInitialState);
			for (const EntityStateMachine::Transition& transition : source.entityStateMachineTransitions)
			{
				const std::vector<EntityStateMachine::Condition> conditions = transition.conditions.empty()
					? std::vector<EntityStateMachine::Condition>{ transition.condition } : transition.conditions;
				stateMachine->AddTransition(transition.from, transition.to, transition.blendSeconds,
					transition.waitForCurrentStateComplete, conditions);
			}
		}
	}
	Entity* result = scene.AddObject(std::move(instance));
	if (!result)
		return nullptr;

	// Runtime-spawned entities are added after the scene-wide physics
	// registration pass, so register their ordinary hurt/collision volume here.
	PhysicsWorld::Instance().Add(*result);
	result->startUp();
	result->FirstFrameComponents();
	m_instances.push_back(result);
	return result;
}

bool SpawnManager::Despawn(Scene& scene, Entity* entity)
{
	if (!entity)
		return false;
	const auto found = std::find(m_instances.begin(), m_instances.end(), entity);
	if (found == m_instances.end())
		return false;
	if (!scene.RemoveObject(entity))
		return false;
	m_instances.erase(found);
	return true;
}

void SpawnManager::DespawnRuntimeInstances(Scene& scene)
{
	for (auto iterator = m_instances.begin(); iterator != m_instances.end();)
	{
		Entity* entity = *iterator;
		if (!entity || scene.RemoveObject(entity))
		{
			iterator = m_instances.erase(iterator);
		}
		else
		{
			++iterator;
		}
	}
}
