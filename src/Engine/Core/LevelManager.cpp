#include "Engine/Core/LevelManager.h"

#include "Engine/Core/AnimatorComponent.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/PlayerController.h"
#include "Engine/Core/ProjectStateData.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <algorithm>

void LevelManager::Clear()
{
	m_levels.clear();
	m_activeLevel = nullptr;
	m_editorTransformSnapshots.clear();
	m_startupLevelName.clear();
}

LevelManager::~LevelManager() = default;

Level* LevelManager::startUp()
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

Level* LevelManager::CreateLevel(std::string name)
{
	auto level = std::make_unique<Level>(std::move(name));
	Level* rawLevel = level.get();
	m_levels.push_back(std::move(level));
	if (!m_activeLevel)
	{
		m_activeLevel = rawLevel;
	}
	return rawLevel;
}

Level* LevelManager::FindLevel(const std::string& name) const
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

bool LevelManager::SetActiveLevel(const std::string& name)
{
	Level* level = FindLevel(name);
	if (!level)
	{
		return false;
	}

	m_activeLevel = level;
	return true;
}

void LevelManager::SetStartupLevelName(std::string name)
{
	m_startupLevelName = std::move(name);
}

const std::string& LevelManager::StartupLevelName() const
{
	return m_startupLevelName;
}

void LevelManager::AppendProjectState(std::string& contents) const
{
	if (m_startupLevelName.empty())
	{
		return;
	}

	contents += "startuplevel;";
	contents += m_startupLevelName;
	contents += "\n";
}

void LevelManager::ApplyProjectState(const std::string& startupLevelName)
{
	m_startupLevelName = startupLevelName;
}

void LevelManager::ApplyProjectState(
	const std::vector<ProjectStateData::PendingLevel>& pendingLevels,
	const std::vector<ProjectStateData::PendingController>& pendingControllers,
	const std::vector<ProjectStateData::PendingComponent>& pendingComponents)
{
	auto findOrCreateLevel = [this](const std::string& levelName) -> Level*
	{
		if (Level* level = FindLevel(levelName))
		{
			return level;
		}
		return CreateLevel(levelName);
	};

	for (const auto& pendingController : pendingControllers)
	{
		Level* level = findOrCreateLevel(pendingController.levelName);
		if (!level)
		{
			continue;
		}

		for (const auto& object : level->Objects())
		{
			if (!object || object->SourcePath() != pendingController.sourcePath.string())
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
				}
			}
		}
	}

	for (const auto& pendingComponent : pendingComponents)
	{
		Level* level = findOrCreateLevel(pendingComponent.levelName);
		if (!level)
		{
			continue;
		}

		for (const auto& object : level->Objects())
		{
			if (!object || object->SourcePath() != pendingComponent.sourcePath.string())
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
						animator->AddTransition(transition.from, transition.to, transition.blendSeconds, condition);
					}
				}
			}
		}
	}

	if (!pendingLevels.empty())
	{
		const Level* activeLevel = nullptr;
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
		SetActiveLevel(activeLevel->Name());
	}
}

Level* LevelManager::ActiveLevel()
{
	return m_activeLevel;
}

const Level* LevelManager::ActiveLevel() const
{
	return m_activeLevel;
}

void LevelManager::ResetActiveLevelEntitiesToDefaultPosition()
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

void LevelManager::CaptureActiveLevelEditorTransforms()
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

void LevelManager::RestoreActiveLevelEditorTransforms()
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
