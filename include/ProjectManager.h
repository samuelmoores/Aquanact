#pragma once

#include <filesystem>

class SceneManager;

class ProjectManager {
public:
	ProjectManager() = default;

	bool SaveProject(const std::filesystem::path& path, const SceneManager& sceneManager) const;
	bool LoadProject(const std::filesystem::path& path, SceneManager& sceneManager) const;
};
