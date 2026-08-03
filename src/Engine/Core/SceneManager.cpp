#include "Engine/Core/SceneManager.h"

#include "Engine/Core/AnimatorComponent.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/PlayerController.h"
#include "Engine/Core/ProjectStateData.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <algorithm>

namespace
{
	bool MatchesComponentOwner(const Entity& object, unsigned int entityId, const std::filesystem::path& sourcePath)
	{
		return entityId != 0
			? object.Id() == entityId
			: object.SourcePath() == sourcePath.string();
	}
}

void SceneManager::Clear()
{
	m_levels.clear();
	m_sceneKinds.clear();
	m_activeLevel = nullptr;
	m_editorTransformSnapshots.clear();
	m_startupLevelName.clear();
}

SceneManager::~SceneManager() = default;

Scene* SceneManager::startUp()
{
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
		m_activeLevel->startUp();
	}

	return m_activeLevel;
}

Scene* SceneManager::CreateLevel(std::string name)
{
	auto level = std::make_unique<Scene>(std::move(name));
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
	auto level = std::make_unique<Scene>(std::move(name));
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
	for (const auto& level : m_levels)
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
	const auto it = m_sceneKinds.find(name);
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
	for (const auto& level : m_levels)
	{
		if (level && SceneKindFor(level->Name()) == kind)
		{
			names.push_back(level->Name());
		}
	}
	return names;
}

void SceneManager::SetStartupLevelName(std::string name)
{
	m_startupLevelName = std::move(name);
}

const std::string& SceneManager::StartupLevelName() const
{
	return m_startupLevelName;
}

void SceneManager::AppendProjectState(std::string& contents) const
{
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
	auto findOrCreateLevel = [this](const std::string& levelName) -> Scene*
	{
		if (Scene* level = FindLevel(levelName))
		{
			return level;
		}
		return CreateLevel(levelName);
	};

	for (const auto& pendingController : pendingControllers)
	{
		Scene* level = findOrCreateLevel(pendingController.levelName);
		if (!level)
		{
			continue;
		}

		for (const auto& object : level->Objects())
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

	for (const auto& pendingComponent : pendingComponents)
	{
		Scene* level = findOrCreateLevel(pendingComponent.levelName);
		if (!level)
		{
			continue;
		}

		for (const auto& object : level->Objects())
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
					for (const auto& state : pendingComponent.animatorStates)
					{
						animator->AddState(state.name, state.clipIndex);
					}
					for (const auto& transition : pendingComponent.animatorTransitions)
					{
						std::vector<AnimatorComponent::Condition> conditions;
						if (!transition.conditions.empty())
						{
							for (const auto& conditionData : transition.conditions)
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
		for (const auto& pendingLevel : pendingLevels)
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
		for (const auto& pendingLevel : pendingLevels)
		{
			SetSceneKind(pendingLevel.name, pendingLevel.isCutscene ? SceneKind::Cutscene : SceneKind::Level);
		}
		SetActiveLevel(activeLevel->Name());
	}
}

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

	for (const auto& object : m_activeLevel->Objects())
	{
		if (object)
		{
			object->ResetToDefaultPosition();
		}
	}
}

void SceneManager::CaptureActiveLevelEditorTransforms()
{
	m_editorTransformSnapshots.clear();
	if (!m_activeLevel)
	{
		return;
	}

	for (const auto& object : m_activeLevel->Objects())
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

	for (const auto& object : m_activeLevel->Objects())
	{
		if (!object)
		{
			continue;
		}

		const auto it = m_editorTransformSnapshots.find(object.get());
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
