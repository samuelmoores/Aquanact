#pragma once

#include <filesystem>

class LevelManager;
class FileSystem;

class ProjectManager {
public:
	explicit ProjectManager(FileSystem& fileSystem);

	bool SaveProject(const std::filesystem::path& path, const LevelManager& levelManager) const;
	bool LoadProject(const std::filesystem::path& path, LevelManager& levelManager) const;

	void SetStartupLevelName(std::string name);
	const std::string& StartupLevelName() const;

private:
	FileSystem* m_fileSystem = nullptr;
	mutable std::string m_startupLevelName;
};
