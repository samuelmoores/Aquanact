#include "Engine/UI/InstanceManager.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/SpawnManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Component.h"
#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace
{
	bool SamePath(const std::filesystem::path& left, const std::filesystem::path& right)
	{
		std::error_code leftError;
		std::error_code rightError;
		const auto leftCanonical = std::filesystem::weakly_canonical(left, leftError);
		const auto rightCanonical = std::filesystem::weakly_canonical(right, rightError);
		return (!leftError && !rightError ? leftCanonical : left).generic_string()
			== (!leftError && !rightError ? rightCanonical : right).generic_string();
	}

	bool IsModelFile(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return extension == ".fbx" || extension == ".obj" || extension == ".gltf"
			|| extension == ".glb" || extension == ".dae";
	}
}

InstanceManager::~InstanceManager() = default;

void InstanceManager::SyncComponentPreview()
{
	if (m_selectedModel < 0 || m_selectedModel >= static_cast<int>(m_models.size()))
	{
		m_componentPreview.reset();
		return;
	}

	const std::filesystem::path& modelPath = m_models[static_cast<std::size_t>(m_selectedModel)];
	bool rebuild = !m_componentPreview || !SamePath(m_componentPreview->SourcePath(), modelPath);
	if (!rebuild)
	{
		std::vector<std::string> previewTypes;
		for (Component* component : m_componentPreview->Components())
			if (component && !dynamic_cast<EntityStateMachine*>(component)) previewTypes.push_back(component->Name());
		std::vector<std::string> requestedTypes;
		for (const std::string& componentType : m_componentTypes)
			if (componentType != "EntityStateMachine") requestedTypes.push_back(componentType);
		rebuild = previewTypes != requestedTypes;
	}
	if (!rebuild)
		return;

	m_componentPreview = std::make_unique<Entity>(modelPath.string().c_str(), false);
	if (!m_componentPreview->GetMesh())
	{
		m_componentPreview.reset();
		return;
	}
	m_componentPreview->SetName(m_entityName);
	for (const std::string& componentType : m_componentTypes)
	{
		if (componentType == "EntityStateMachine" || m_componentPreview->GetComponentByName(componentType)) continue;
		if (std::unique_ptr<Component> component = ComponentFactory::Instance().Create(componentType, *m_componentPreview))
			m_componentPreview->AddComponent(std::move(component));
	}
}

void InstanceManager::DrawComponentPreview()
{
	if (!m_componentPreview)
	{
		ImGui::TextDisabled("Select a model to configure components.");
		return;
	}

	const std::vector<Component*> components = m_componentPreview->Components();
	for (std::size_t index = 0; index < components.size(); ++index)
	{
		Component* component = components[index];
		if (!component || dynamic_cast<EntityStateMachine*>(component)) continue;
		if (index > 0) ImGui::Separator();
		ImGui::PushID(component);
		const std::string popupId = "Remove Instance Component##Confirm_"
			+ std::to_string(reinterpret_cast<std::uintptr_t>(component));
		const EntityComponentHeaderResult header = m_entityWindow.DrawComponentHeader(*component);
		if (header.removeRequested) ImGui::OpenPopup(popupId.c_str());
		if (header.expanded) m_entityWindow.DrawComponentControls(*component);
		if (m_entityWindow.DrawRemoveComponentPopup(*m_componentPreview, *component, popupId.c_str()))
		{
			m_componentPreview->RemoveComponent(component);
			m_componentTypes.clear();
			for (Component* remaining : m_componentPreview->Components())
				if (remaining && !dynamic_cast<EntityStateMachine*>(remaining)) m_componentTypes.push_back(remaining->Name());
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}
}

namespace
{
	void ApplyStateMachineConfiguration(EntityStateMachine& machine, const InstanceDefinition& definition)
	{
		for (const EntityStateMachine::State& state : definition.entityStateMachineStates)
		{
			const bool waitForCompletion = state.waitForCompletion || std::any_of(
				definition.entityStateMachineTransitions.begin(), definition.entityStateMachineTransitions.end(),
				[&state](const auto& transition)
				{
					return transition.from == state.name && transition.waitForCurrentStateComplete;
				});
			std::string animationName = state.animationName;
			int animationIndex = machine.FindAnimationIndex(animationName);
			if (animationIndex < 0)
				animationIndex = machine.FindAnimationIndex(state.name);
			if (animationIndex >= 0)
				animationName = machine.AnimationNames()[static_cast<std::size_t>(animationIndex)];
			std::vector<std::string> animationSequence;
			for (const std::string& animation : state.animationSequence)
			{
				const int sequenceIndex = machine.FindAnimationIndex(animation);
				animationSequence.push_back(sequenceIndex >= 0
					? machine.AnimationNames()[static_cast<std::size_t>(sequenceIndex)] : animation);
			}
			if (!machine.AddState(state.name, std::move(animationName), state.blocksMovement, state.blocksInput,
				state.useAnimationSequence, std::move(animationSequence), waitForCompletion, state.loop))
				continue;
			for (const EntityStateMachine::SoundEvent& event : state.soundEvents)
				machine.AddStateSoundEvent(state.name, event);
		}
		machine.SetInitialState(definition.entityStateMachineInitialState);
		for (const EntityStateMachine::Transition& transition : definition.entityStateMachineTransitions)
		{
			const std::vector<EntityStateMachine::Condition> conditions = transition.conditions.empty()
				? std::vector<EntityStateMachine::Condition>{ transition.condition } : transition.conditions;
			machine.AddTransition(transition.from, transition.to, transition.blendSeconds,
				transition.waitForCurrentStateComplete, conditions);
		}
	}
}

void InstanceManager::RefreshModels()
{
	m_models.clear();
	m_selectedModel = -1;
	const std::filesystem::path modelRoot = Root::Current().Projects().ProjectAssetsDirectory() / "models";
	std::error_code error;
	if (!std::filesystem::is_directory(modelRoot, error))
		return;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(modelRoot, error))
	{
		if (!error && entry.is_regular_file(error) && IsModelFile(entry.path()))
			m_models.push_back(entry.path());
	}
	std::sort(m_models.begin(), m_models.end());
	if (!m_models.empty())
		m_selectedModel = 0;
}

void BeginInstanceEdit(
	const InstanceDefinition& definition,
	std::vector<std::filesystem::path>& models,
	int& selectedModel,
	char* entityName,
	std::size_t entityNameSize,
	std::vector<std::string>& componentTypes,
	std::string& editingName)
{
	std::strncpy(entityName, definition.name.c_str(), entityNameSize - 1);
	entityName[entityNameSize - 1] = '\0';
	componentTypes = definition.componentTypes;
	editingName = definition.name;
	selectedModel = -1;
	for (int index = 0; index < static_cast<int>(models.size()); ++index)
	{
		if (SamePath(models[static_cast<std::size_t>(index)], definition.modelPath))
		{
			selectedModel = index;
			break;
		}
	}
}

bool InstanceManager::OpenStateMachinePreview(const InstanceDefinition& definition)
{
	if (definition.componentTypes.end() == std::find(definition.componentTypes.begin(), definition.componentTypes.end(), "EntityStateMachine"))
		return false;
	m_stateMachinePreview = std::make_unique<Entity>(definition.modelPath.c_str(), false);
	if (!m_stateMachinePreview->GetMesh() || !m_stateMachinePreview->GetMesh()->Skinned())
	{
		m_stateMachinePreview.reset();
		return false;
	}
	m_stateMachinePreview->SetName(definition.name + " State Machine Preview");
	for (const std::string& componentType : definition.componentTypes)
	{
		if (componentType == "EntityStateMachine"
			|| m_stateMachinePreview->GetComponentByName(componentType))
			continue;
		if (std::unique_ptr<Component> component = ComponentFactory::Instance().Create(
			componentType, *m_stateMachinePreview))
			m_stateMachinePreview->AddComponent(std::move(component));
	}
	m_stateMachinePreview->AddComponent<EntityStateMachine>(m_stateMachinePreview->GetMesh());
	ApplyStateMachineConfiguration(*m_stateMachinePreview->GetEntityState(), definition);
	m_stateMachinePreviewName = definition.name;
	return true;
}

void InstanceManager::SyncStateMachineConfiguration()
{
	if (!m_stateMachinePreview || m_stateMachinePreviewName.empty())
		return;
	InstanceDefinition* definition = Root::Current().Spawns().FindDefinition(m_stateMachinePreviewName);
	if (!definition || !m_stateMachinePreview->GetEntityState())
		return;
	const EntityStateMachine& machine = *m_stateMachinePreview->GetEntityState();
	definition->hasEntityStateMachineConfiguration = true;
	definition->entityStateMachineInitialState = machine.InitialState();
	definition->entityStateMachineStates = machine.States();
	definition->entityStateMachineTransitions = machine.Transitions();
}

EntityStateMachine* InstanceManager::StateMachinePreview() const
{
	return m_stateMachinePreview ? m_stateMachinePreview->GetEntityState() : nullptr;
}

InstanceManagerResult InstanceManager::Draw(SceneManager& scenes, bool& open)
{
	InstanceManagerResult result;
	(void)scenes;
	if (!open)
	{
		result.stateMachineToDraw = StateMachinePreview();
		return result;
	}

	if (!ImGui::Begin("Instance Manager", &open))
	{
		ImGui::End();
		result.stateMachineToDraw = StateMachinePreview();
		return result;
	}
	if (m_models.empty())
		RefreshModels();
	SyncComponentPreview();

	ImGui::TextUnformatted("Create Entity Instance");
	ImGui::TextDisabled("Choose a model and configure the components on the instance.");
	if (ImGui::SmallButton("Refresh Models"))
		RefreshModels();
	if (ImGui::BeginCombo("Model", m_selectedModel >= 0 ? m_models[m_selectedModel].filename().string().c_str() : "Select model"))
	{
		for (int index = 0; index < static_cast<int>(m_models.size()); ++index)
		{
			const bool selected = index == m_selectedModel;
			const std::string label = m_models[index].lexically_relative(
				Root::Current().Projects().ProjectAssetsDirectory() / "models").generic_string();
			if (ImGui::Selectable(label.c_str(), selected))
				m_selectedModel = index;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::InputText("Entity Name", m_entityName, sizeof(m_entityName));
	if (!m_editingDefinitionName.empty())
	{
		ImGui::TextDisabled("Editing: %s", m_editingDefinitionName.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Cancel Edit"))
		{
			m_editingDefinitionName.clear();
			m_componentTypes.clear();
		}
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Components");
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::BeginCombo("##AddInstanceComponent", "Add Component"))
	{
		const std::vector<std::string> names = ComponentFactory::Instance().Names();
		for (const std::string& name : names)
		{
			const bool alreadyAttached = std::find(m_componentTypes.begin(), m_componentTypes.end(), name) != m_componentTypes.end();
			ImGui::BeginDisabled(alreadyAttached);
			if (ImGui::Selectable(name.c_str(), false) && !alreadyAttached)
			{
				m_componentTypes.push_back(name);
				if (m_componentPreview && (name != "EntityStateMachine")
					&& !m_componentPreview->GetComponentByName(name))
				{
					if (std::unique_ptr<Component> component = ComponentFactory::Instance().Create(name, *m_componentPreview))
						m_componentPreview->AddComponent(std::move(component));
				}
			}
			ImGui::EndDisabled();
		}
		if (names.empty())
		{
			ImGui::Separator();
			ImGui::TextDisabled("No component types are registered.");
		}
		ImGui::EndCombo();
	}
	DrawComponentPreview();
	if (ImGui::Button(m_editingDefinitionName.empty() ? "Create Instance" : "Update Instance"))
	{
		if (m_selectedModel < 0 || m_selectedModel >= static_cast<int>(m_models.size()))
			std::snprintf(m_statusMessage, sizeof(m_statusMessage), "Select a model.");
		else if (!std::string(m_entityName).size())
			std::snprintf(m_statusMessage, sizeof(m_statusMessage), "Enter an entity name.");
		else
		{
			InstanceDefinition definition;
			definition.name = m_entityName;
			definition.modelPath = m_models[m_selectedModel].string();
			definition.componentTypes = m_componentTypes;
			if (!m_editingDefinitionName.empty())
			{
				if (const InstanceDefinition* previous = Root::Current().Spawns().FindDefinition(m_editingDefinitionName))
				{
					definition.hasEntityStateMachineConfiguration = previous->hasEntityStateMachineConfiguration;
					definition.entityStateMachineInitialState = previous->entityStateMachineInitialState;
					definition.entityStateMachineStates = previous->entityStateMachineStates;
					definition.entityStateMachineTransitions = previous->entityStateMachineTransitions;
				}
			}
			const std::string previousName = m_editingDefinitionName;
			if (!Root::Current().Spawns().CreateInstance(std::move(definition)))
			{
				std::snprintf(m_statusMessage, sizeof(m_statusMessage), "Could not create instance definition.");
			}
			else
			{
				if (!previousName.empty() && previousName != m_entityName)
					Root::Current().Spawns().DeleteInstance(previousName);
				m_editingDefinitionName.clear();
				std::snprintf(m_statusMessage, sizeof(m_statusMessage), "Created %s.", m_entityName);
			}
		}
	}
	EngineGuiWidgets::StatusMessage(m_statusMessage);
	ImGui::Separator();
	ImGui::Text("Instances: %zu | Active in scene: %zu",
		Root::Current().Spawns().Definitions().size(),
		Root::Current().Spawns().ActiveInstanceCount());
	for (const auto& entry : Root::Current().Spawns().Definitions())
	{
		ImGui::PushID(entry.first.c_str());
		ImGui::Text("%s  (%zu components)", entry.first.c_str(), entry.second.componentTypes.size());
		ImGui::SameLine();
		if (std::find(entry.second.componentTypes.begin(), entry.second.componentTypes.end(), "EntityStateMachine") != entry.second.componentTypes.end())
		{
			if (ImGui::SmallButton("Open State Machine") && OpenStateMachinePreview(entry.second))
				result.openStateMachine = true;
			ImGui::SameLine();
		}
		if (ImGui::SmallButton("Edit"))
			BeginInstanceEdit(entry.second, m_models, m_selectedModel, m_entityName, sizeof(m_entityName),
				m_componentTypes, m_editingDefinitionName);
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete"))
		{
			if (m_stateMachinePreviewName == entry.first)
			{
				m_stateMachinePreview.reset();
				m_stateMachinePreviewName.clear();
				result.closeStateMachine = true;
			}
			Root::Current().Spawns().DeleteInstance(entry.first);
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}
	ImGui::End();
	result.stateMachineToDraw = StateMachinePreview();
	return result;
}
