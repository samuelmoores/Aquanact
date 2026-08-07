#include "Engine/Core/SceneManager.h"

#include "Engine/Core/AnimatorComponent.h"
#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/PlayerController.h"
#include "Engine/Core/ProjectStateData.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <fstream>
#include <iostream>
#include <memory>

struct NewClassConfiguration
{
	std::string className;
	bool attachToExistingEntity = false;
	bool createNewEntity = false;
	std::string targetEntityName;
};

// Helper function to trim spaces and remove the prefix (e.g., "ClassName: ")
std::string extractValue(const std::string& line, const std::string& prefix)
{
	if (line.rfind(prefix, 0) == 0) // Checks if line starts with prefix
	{
		return line.substr(prefix.length());
	}
	return "";
}

NewClassConfiguration loadConfiguration(const std::string& filename)
{
	std::ifstream inFile(filename);
	NewClassConfiguration config;

	if (!inFile)
	{
		std::cerr << "Error: Could not open file " << filename << " for reading.\n";
		return config;
	}

	std::string line;

	// Local variables corresponding to each line in the text file
	std::string localClassName = "";
	std::string localAttachStr = "";
	std::string localCreateStr = "";
	std::string localTargetName = "";

	// Read the file line by line
	while (std::getline(inFile, line))
	{
		if (line.rfind("ClassName: ", 0) == 0)
		{
			localClassName = extractValue(line, "ClassName: ");
		}
		else if (line.rfind("AttachToExistingEntity: ", 0) == 0)
		{
			localAttachStr = extractValue(line, "AttachToExistingEntity: ");
		}
		else if (line.rfind("CreateNewEntity: ", 0) == 0)
		{
			localCreateStr = extractValue(line, "CreateNewEntity: ");
		}
		else if (line.rfind("TargetEntityName: ", 0) == 0)
		{
			localTargetName = extractValue(line, "TargetEntityName: ");
		}
	}

	// Convert local text variables to the final struct types
	config.className = localClassName;
	config.attachToExistingEntity = (localAttachStr == "true");
	config.createNewEntity = (localCreateStr == "true");
	config.targetEntityName = localTargetName;

	return config;
}

namespace
{
	// Match a serialized component target either by stable entity id or by source path
	// when older project data does not yet have ids.
	bool MatchesComponentOwner(const Entity& object, unsigned int entityId, const std::filesystem::path& sourcePath)
	{
		return entityId != 0
			? object.Id() == entityId
			: object.SourcePath() == sourcePath.string();
	}
}

SceneManager::~SceneManager() = default;

// Lifecycle and state reset
Scene* SceneManager::startUp()
{

	// check for new game code
	// load congiguration
	NewClassConfiguration configuration = loadConfiguration("NewClassConfiguration");

	Scene* activeLevel = m_activeLevel;
	if (!activeLevel)
	{
		return m_activeLevel;
	}

	auto attachComponent = [&](Entity& entity) -> bool
	{
		if (Component* existing = entity.GetComponentByName(configuration.className))
		{
			(void)existing;
			return true;
		}

		std::unique_ptr<Component> component = ComponentFactory::Instance().Create(configuration.className, entity);
		if (!component)
		{
			std::cerr << "Error: Unknown component type '" << configuration.className << "'.\n";
			return false;
		}

		entity.AddComponent(std::move(component));
		return true;
	};

	// add new entity or update existing
	if (configuration.createNewEntity)
	{
		// Create entity
		auto newEntity = std::make_unique<Entity>();
		newEntity.get()->SetName(configuration.className);
		Entity* entity = activeLevel->AddObject(std::move(newEntity));

		if (entity)
		{
			attachComponent(*entity);
		}
	}
	else if (configuration.attachToExistingEntity && !configuration.targetEntityName.empty())
	{
		for (const std::unique_ptr<Entity>& entity : activeLevel->Objects())
		{
			if (!entity || entity->Name() != configuration.targetEntityName)
			{
				continue;
			}

			attachComponent(*entity);
			break;
		}
	}

	std::error_code removeEc;
	std::filesystem::remove("NewClassConfiguration", removeEc);

	// ************ start up the active scene *************

	// Choose the active scene from startup config first, then fall back to the first
	// created scene if the configured one does not exist.
	if (!m_activeLevel)
	{
		if (!m_startupLevelName.empty())
		{
			m_activeLevel = FindLevel(m_startupLevelName);
		}
		if (!m_activeLevel && !m_levels.empty())
		{
			m_activeLevel = m_levels.front().get();
		}
	}

	if (m_activeLevel)
	{
		// Scene startup is delegated to the currently active scene.
		m_activeLevel->startUp();
	}

	return m_activeLevel;
}

void SceneManager::Clear()
{
	// Reset all scene-manager state to a fresh startup baseline.
	m_levels.clear();
	m_sceneKinds.clear();
	m_activeLevel = nullptr;
	m_editorTransformSnapshots.clear();
	m_startupLevelName.clear();
}

//Scene creation and lookup
Scene* SceneManager::CreateLevel(std::string name)
{
	// Levels and cutscenes share the same storage; only the recorded kind differs.
	std::unique_ptr<Scene> level = std::make_unique<Scene>(std::move(name));
	Scene* rawLevel = level.get();
	m_levels.push_back(std::move(level));
	m_sceneKinds[rawLevel->Name()] = SceneKind::Level;
	if (!m_activeLevel)
	{
		m_activeLevel = rawLevel;
	}
	return rawLevel;
}

Scene* SceneManager::CreateCutscene(std::string name)
{
	std::unique_ptr<Scene> level = std::make_unique<Scene>(std::move(name));
	Scene* rawLevel = level.get();
	m_levels.push_back(std::move(level));
	m_sceneKinds[rawLevel->Name()] = SceneKind::Cutscene;
	if (!m_activeLevel)
	{
		m_activeLevel = rawLevel;
	}
	return rawLevel;
}

Scene* SceneManager::FindLevel(const std::string& name) const
{
	for (const std::unique_ptr<Scene>& level : m_levels)
	{
		if (level && level->Name() == name)
		{
			return level.get();
		}
	}
	return nullptr;
}

bool SceneManager::SetActiveLevel(const std::string& name)
{
	Scene* level = FindLevel(name);
	if (!level)
	{
		return false;
	}

	m_activeLevel = level;
	return true;
}

void SceneManager::SetSceneKind(const std::string& name, SceneKind kind)
{
	m_sceneKinds[name] = kind;
}

SceneManager::SceneKind SceneManager::SceneKindFor(const std::string& name) const
{
	const std::unordered_map<std::string, SceneKind>::const_iterator it = m_sceneKinds.find(name);
	if (it != m_sceneKinds.end())
	{
		return it->second;
	}
	return SceneKind::Level;
}

bool SceneManager::IsMainMenuScene(const std::string& name) const
{
	return name == "MainMenu";
}

std::vector<std::string> SceneManager::SceneNames(SceneKind kind) const
{
	std::vector<std::string> names;
	for (const std::unique_ptr<Scene>& level : m_levels)
	{
		if (level && SceneKindFor(level->Name()) == kind)
		{
			names.push_back(level->Name());
		}
	}
	return names;
}

// Project state serialization
void SceneManager::AppendProjectState(std::string& contents) const
{
	// Only persist startup state when it has been configured.
	if (m_startupLevelName.empty())
	{
		return;
	}

	contents += "startuplevel;";
	contents += m_startupLevelName;
	contents += "\n";
}

void SceneManager::ApplyProjectState(const std::string& startupLevelName)
{
	m_startupLevelName = startupLevelName;
}

void SceneManager::ApplyProjectState(
	const std::vector<ProjectStateData::PendingLevel>& pendingLevels,
	const std::vector<ProjectStateData::PendingController>& pendingControllers,
	const std::vector<ProjectStateData::PendingComponent>& pendingComponents)
{
	// Project restore needs to resolve level references before applying component data.
	const std::function<Scene*(const std::string&)> findOrCreateLevel = [this](const std::string& levelName) -> Scene*
	{
		if (Scene* level = FindLevel(levelName))
		{
			return level;
		}
		return CreateLevel(levelName);
	};

	for (const ProjectStateData::PendingController& pendingController : pendingControllers)
	{
		Scene* level = findOrCreateLevel(pendingController.levelName);
		if (!level)
		{
			continue;
		}

		for (const std::unique_ptr<Entity>& object : level->Objects())
		{
			if (!object || !MatchesComponentOwner(*object, pendingController.entityId, pendingController.sourcePath))
			{
				continue;
			}
			if (SceneKindFor(level->Name()) == SceneKind::Cutscene)
			{
				continue;
			}

			if (pendingController.playerControlled)
			{
				if (!object->GetComponent<PlayerController>())
				{
					object->AddComponent<PlayerController>();
				}
				if (PlayerController* playerController = object->GetComponent<PlayerController>())
				{
					playerController->SetMoveSpeed(pendingController.moveSpeed);
					playerController->SetMovementDeadzone(pendingController.movementDeadzone);
					playerController->SetTurnSpeed(pendingController.turnSpeed);
				}
			}
			else
			{
				if (!object->GetController())
				{
					object->AddComponent<Controller>();
				}
				if (Controller* controller = object->GetController())
				{
					controller->SetMoveSpeed(pendingController.moveSpeed);
					controller->SetMovementDeadzone(pendingController.movementDeadzone);
				}
			}
		}
	}

	for (const ProjectStateData::PendingComponent& pendingComponent : pendingComponents)
	{
		Scene* level = findOrCreateLevel(pendingComponent.levelName);
		if (!level)
		{
			continue;
		}

		for (const std::unique_ptr<Entity>& object : level->Objects())
		{
			if (!object || !MatchesComponentOwner(*object, pendingComponent.entityId, pendingComponent.sourcePath))
			{
				continue;
			}
			if (SceneKindFor(level->Name()) == SceneKind::Cutscene)
			{
				continue;
			}

			if (pendingComponent.type == "playerhealth")
			{
				if (!object->GetComponent<PlayerHealth>())
				{
					object->AddComponent<PlayerHealth>();
				}
				if (PlayerHealth* playerHealth = object->GetComponent<PlayerHealth>())
				{
					if (pendingComponent.hasValue1)
					{
						playerHealth->SetHealth(pendingComponent.value1);
					}
					if (pendingComponent.hasValue2)
					{
						playerHealth->SetMaxHealth(pendingComponent.value2);
					}
				}
			}
			else if (pendingComponent.type == "enemy")
			{
				if (!object->GetComponent<Enemy>())
				{
					object->AddComponent<Enemy>();
				}
			}
			else if (pendingComponent.type == "animator")
			{
				if (!object->GetComponent<AnimatorComponent>())
				{
					object->AddComponent<AnimatorComponent>(object->GetMesh());
				}
				if (AnimatorComponent* animator = object->GetComponent<AnimatorComponent>())
				{
					animator->SetInitialState(pendingComponent.initialState);
					for (const ProjectStateData::PendingComponent::AnimatorStateData& state : pendingComponent.animatorStates)
					{
						animator->AddState(state.name, state.clipIndex);
					}
					for (const ProjectStateData::PendingComponent::AnimatorTransitionData& transition : pendingComponent.animatorTransitions)
					{
						std::vector<AnimatorComponent::Condition> conditions;
						if (!transition.conditions.empty())
						{
							// Convert serialized animator conditions back into live AnimatorComponent conditions.
							for (const ProjectStateData::PendingComponent::AnimatorConditionData& conditionData : transition.conditions)
							{
								AnimatorComponent::Condition condition;
								condition.left.type = static_cast<AnimatorComponent::OperandType>(conditionData.left.type);
								condition.left.constantValue = conditionData.left.constantValue;
								condition.left.componentName = conditionData.left.componentName;
								condition.left.memberName = conditionData.left.memberName;
								condition.comparator = static_cast<AnimatorComponent::Comparator>(conditionData.comparator);
								condition.right.type = static_cast<AnimatorComponent::OperandType>(conditionData.right.type);
								condition.right.constantValue = conditionData.right.constantValue;
								condition.right.componentName = conditionData.right.componentName;
								condition.right.memberName = conditionData.right.memberName;
								conditions.push_back(std::move(condition));
							}
						}
						else
						{
							AnimatorComponent::Condition condition;
							condition.left.type = static_cast<AnimatorComponent::OperandType>(transition.left.type);
							condition.left.constantValue = transition.left.constantValue;
							condition.left.componentName = transition.left.componentName;
							condition.left.memberName = transition.left.memberName;
							condition.comparator = static_cast<AnimatorComponent::Comparator>(transition.comparator);
							condition.right.type = static_cast<AnimatorComponent::OperandType>(transition.right.type);
							condition.right.constantValue = transition.right.constantValue;
							condition.right.componentName = transition.right.componentName;
							condition.right.memberName = transition.right.memberName;
							conditions.push_back(std::move(condition));
						}
						animator->AddTransition(transition.from, transition.to, transition.blendSeconds, std::move(conditions));
					}
				}
			}
		}
	}

	if (!pendingLevels.empty())
	{
		const Scene* activeLevel = nullptr;
		// Rebuild the scene kind map first, then re-activate the saved scene.
		for (const ProjectStateData::PendingLevel& pendingLevel : pendingLevels)
		{
			if (pendingLevel.active)
			{
				activeLevel = FindLevel(pendingLevel.name);
				break;
			}
		}
		if (!activeLevel)
		{
			activeLevel = m_levels.front().get();
		}
		for (const ProjectStateData::PendingLevel& pendingLevel : pendingLevels)
		{
			SetSceneKind(pendingLevel.name, pendingLevel.isCutscene ? SceneKind::Cutscene : SceneKind::Level);
		}
		SetActiveLevel(activeLevel->Name());
	}
}

// Active scene access and other editor helpers
Scene* SceneManager::ActiveLevel()
{
	return m_activeLevel;
}

const Scene* SceneManager::ActiveLevel() const
{
	return m_activeLevel;
}

void SceneManager::ResetActiveLevelEntitiesToDefaultPosition()
{
	if (!m_activeLevel)
	{
		return;
	}

	for (const std::unique_ptr<Entity>& object : m_activeLevel->Objects())
	{
		if (object)
		{
			object->ResetToDefaultPosition();
		}
	}
}

void SceneManager::CaptureActiveLevelEditorTransforms()
{
	// Snapshot the current editor transform state so it can be restored after
	// gameplay/runtime edits are applied.
	m_editorTransformSnapshots.clear();
	if (!m_activeLevel)
	{
		return;
	}

	for (const std::unique_ptr<Entity>& object : m_activeLevel->Objects())
	{
		if (object)
		{
			m_editorTransformSnapshots[object.get()] = {
				object->Position(),
				object->Rotation(),
				object->Scale(),
			};
		}
	}
}

void SceneManager::RestoreActiveLevelEditorTransforms()
{
	if (!m_activeLevel)
	{
		return;
	}

	for (const std::unique_ptr<Entity>& object : m_activeLevel->Objects())
	{
		if (!object)
		{
			continue;
		}

		const std::unordered_map<Entity*, EditorTransformSnapshot>::const_iterator it = m_editorTransformSnapshots.find(object.get());
		if (it == m_editorTransformSnapshots.end())
		{
			continue;
		}

		object->SetRotation(it->second.rotation);
		object->SetScale(it->second.scale);
		object->Translate(it->second.position - object->Position());
	}

	m_editorTransformSnapshots.clear();
}

// Startupconfigurations
void SceneManager::SetStartupLevelName(std::string name)
{
	m_startupLevelName = std::move(name);
}

const std::string& SceneManager::StartupLevelName() const
{
	return m_startupLevelName;
}
