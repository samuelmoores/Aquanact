#pragma once

#include "Engine/Core/Level.h"
#include "Engine/Core/ProjectStateData.h"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Entity;

class LevelManager
{
public:
	LevelManager() = default;
	~LevelManager();

	Level* startUp();
	void Clear();
	Level* CreateLevel(std::string name);
	Level* FindLevel(const std::string& name) const;
	bool SetActiveLevel(const std::string& name);
	void SetStartupLevelName(std::string name);
	const std::string& StartupLevelName() const;
	void AppendProjectState(std::string& contents) const;
	void ApplyProjectState(const std::string& startupLevelName);
	void ApplyProjectState(
		const std::vector<ProjectStateData::PendingLevel>& pendingLevels,
		const std::vector<ProjectStateData::PendingController>& pendingControllers,
		const std::vector<ProjectStateData::PendingComponent>& pendingComponents);
	Level* ActiveLevel();
	const Level* ActiveLevel() const;
	void ResetActiveLevelEntitiesToDefaultPosition();
	void CaptureActiveLevelEditorTransforms();
	void RestoreActiveLevelEditorTransforms();
	const std::vector<std::unique_ptr<Level>>& Levels() const { return m_levels; }

private:
	struct EditorTransformSnapshot
	{
		glm::vec3 position{0.0f};
		glm::vec3 rotation{0.0f};
		glm::vec3 scale{1.0f};
	};

	std::vector<std::unique_ptr<Level>> m_levels;
	Level* m_activeLevel = nullptr;
	std::unordered_map<Entity*, EditorTransformSnapshot> m_editorTransformSnapshots;
	std::string m_startupLevelName;
};

