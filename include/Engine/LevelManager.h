#pragma once

#include "Engine/Level.h"

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
};
