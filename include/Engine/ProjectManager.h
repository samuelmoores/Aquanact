#pragma once

#include <filesystem>

class LevelManager;
class FileSystem;

class ProjectManager {
public:
	explicit ProjectManager(FileSystem& fileSystem);

	bool SaveProject(const std::filesystem::path& path, const LevelManager& levelManager);
	bool LoadProject(const std::filesystem::path& path, LevelManager& levelManager);
	const std::filesystem::path& CurrentProjectPath() const;

private:
	FileSystem* m_fileSystem = nullptr;
	std::filesystem::path m_currentProjectPath;
};
