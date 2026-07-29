#include "Engine/LevelManager.h"

#include "Engine/Level.h"
#include "Engine/Entity.h"

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
