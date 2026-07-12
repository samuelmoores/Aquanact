#pragma once

#include <filesystem>
#include <vector>

class FileManager {
public:
	FileManager() = default;

	void startUp();
	void shutDown();

	void SetRootDirectory(const std::filesystem::path& rootDirectory);
	void SetCurrentDirectory(const std::filesystem::path& currentDirectory);
	void SelectPath(const std::filesystem::path& path);
	void GoUpOneDirectory();
	void Refresh();

	const std::filesystem::path& RootDirectory() const;
	const std::filesystem::path& CurrentDirectory() const;
	const std::filesystem::path& SelectedPath() const;

	bool HasSelection() const;
	bool CanImportSelection() const;
	bool ImportSelected();

	const std::vector<std::filesystem::directory_entry>& Entries() const;

private:
	void ClearEntries();

	std::filesystem::path m_rootDirectory;
	std::filesystem::path m_currentDirectory;
	std::filesystem::path m_selectedPath;
	std::vector<std::filesystem::directory_entry> m_entries;
};
