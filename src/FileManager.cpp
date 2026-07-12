#include "FileManager.h"

#include <iostream>

void FileManager::startUp()
{
	m_rootDirectory = std::filesystem::current_path() / "assets";
	m_currentDirectory = m_rootDirectory;
	m_selectedPath.clear();
	Refresh();
}

void FileManager::shutDown()
{
	ClearEntries();
	m_selectedPath.clear();
	m_currentDirectory.clear();
	m_rootDirectory.clear();
}

void FileManager::SetRootDirectory(const std::filesystem::path& rootDirectory)
{
	m_rootDirectory = rootDirectory;
	if (m_currentDirectory.empty())
	{
		m_currentDirectory = m_rootDirectory;
	}
	Refresh();
}

void FileManager::SetCurrentDirectory(const std::filesystem::path& currentDirectory)
{
	if (currentDirectory.empty() || !std::filesystem::exists(currentDirectory) || !std::filesystem::is_directory(currentDirectory))
	{
		return;
	}

	m_currentDirectory = currentDirectory;
	Refresh();
}

void FileManager::SelectPath(const std::filesystem::path& path)
{
	m_selectedPath = path;
}

void FileManager::GoUpOneDirectory()
{
	if (m_currentDirectory.empty() || m_currentDirectory == m_rootDirectory)
	{
		return;
	}

	const auto parent = m_currentDirectory.parent_path();
	if (!parent.empty())
	{
		SetCurrentDirectory(parent);
	}
}

void FileManager::Refresh()
{
	ClearEntries();

	if (m_currentDirectory.empty() || !std::filesystem::exists(m_currentDirectory) || !std::filesystem::is_directory(m_currentDirectory))
	{
		return;
	}

	for (const auto& entry : std::filesystem::directory_iterator(m_currentDirectory))
	{
		m_entries.push_back(entry);
	}
}

const std::filesystem::path& FileManager::RootDirectory() const
{
	return m_rootDirectory;
}

const std::filesystem::path& FileManager::CurrentDirectory() const
{
	return m_currentDirectory;
}

const std::filesystem::path& FileManager::SelectedPath() const
{
	return m_selectedPath;
}

bool FileManager::HasSelection() const
{
	return !m_selectedPath.empty();
}

bool FileManager::CanImportSelection() const
{
	return HasSelection() && m_selectedPath.extension() == ".fbx";
}

bool FileManager::ImportSelected()
{
	if (!CanImportSelection())
	{
		return false;
	}

	std::cout << "Importing FBX: " << m_selectedPath.string() << std::endl;
	return true;
}

const std::vector<std::filesystem::directory_entry>& FileManager::Entries() const
{
	return m_entries;
}

void FileManager::ClearEntries()
{
	m_entries.clear();
}
