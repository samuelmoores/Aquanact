#pragma once

#include <filesystem>

class LevelManager;
class FileSystem;

class ProjectManager {
public:
	explicit ProjectManager(FileSystem& fileSystem);

	bool SaveProject(const std::filesystem::path& path, const LevelManager& levelManager) const;
	bool LoadProject(const std::filesystem::path& path, LevelManager& levelManager) const;

private:
	FileSystem* m_fileSystem = nullptr;
};
