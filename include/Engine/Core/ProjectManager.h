#pragma once

#include <filesystem>

class SceneManager;
class FileSystem;

class ProjectManager {
public:
	explicit ProjectManager(FileSystem& fileSystem);

	bool SaveProject(const std::filesystem::path& path, const SceneManager& SceneManager);
	bool LoadProject(const std::filesystem::path& path, SceneManager& SceneManager);
	const std::filesystem::path& CurrentProjectPath() const;

private:
	FileSystem* m_fileSystem = nullptr;
	std::filesystem::path m_currentProjectPath;
};


