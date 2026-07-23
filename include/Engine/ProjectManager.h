#pragma once

#include <filesystem>

class SceneManager;
class FileSystem;

class ProjectManager {
public:
	explicit ProjectManager(FileSystem& fileSystem);

	bool SaveProject(const std::filesystem::path& path, const SceneManager& sceneManager) const;
	bool LoadProject(const std::filesystem::path& path, SceneManager& sceneManager) const;

private:
	FileSystem* m_fileSystem = nullptr;
};
