#pragma once

#include "Engine/Core/Scene.h"
#include "Engine/Core/ProjectStateData.h"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Entity;

class SceneManager
{
public:
	enum class SceneKind
	{
		Level,
		Cutscene
	};

	SceneManager() = default;
	~SceneManager();

	Scene* startUp();
	void Clear();
	Scene* CreateLevel(std::string name);
	Scene* CreateCutscene(std::string name);
	Scene* FindLevel(const std::string& name) const;
	bool SetActiveLevel(const std::string& name);
	void SetStartupLevelName(std::string name);
	const std::string& StartupLevelName() const;
	void SetSceneKind(const std::string& name, SceneKind kind);
	SceneKind SceneKindFor(const std::string& name) const;
	bool IsMainMenuScene(const std::string& name) const;
	std::vector<std::string> SceneNames(SceneKind kind) const;
	void AppendProjectState(std::string& contents) const;
	void ApplyProjectState(const std::string& startupLevelName);
	void ApplyProjectState(
		const std::vector<ProjectStateData::PendingLevel>& pendingLevels,
		const std::vector<ProjectStateData::PendingController>& pendingControllers,
		const std::vector<ProjectStateData::PendingComponent>& pendingComponents);
	Scene* ActiveLevel();
	const Scene* ActiveLevel() const;
	void ResetActiveLevelEntitiesToDefaultPosition();
	void CaptureActiveLevelEditorTransforms();
	void RestoreActiveLevelEditorTransforms();
	const std::vector<std::unique_ptr<Scene>>& Levels() const { return m_levels; }

private:
	struct EditorTransformSnapshot
	{
		glm::vec3 position{0.0f};
		glm::vec3 rotation{0.0f};
		glm::vec3 scale{1.0f};
	};

	std::vector<std::unique_ptr<Scene>> m_levels;
	std::unordered_map<std::string, SceneKind> m_sceneKinds;
	Scene* m_activeLevel = nullptr;
	std::unordered_map<Entity*, EditorTransformSnapshot> m_editorTransformSnapshots;
	std::string m_startupLevelName;
};





