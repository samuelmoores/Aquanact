#include "Engine/Core/FileManager.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/LevelManager.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/FileSystem.h"

FileManager::FileManager(FileSystem& fileSystem)
	: m_fileSystem(&fileSystem)
{
}

void FileManager::startUp()
{
	if (!Root::Current().State().IsEditorMode())
	{
		return;
	}

	m_rootDirectory = m_fileSystem ? m_fileSystem->Path("C:/dev/Aquanact/assets/models") : std::filesystem::path("C:/dev/Aquanact/assets/models");
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
	m_currentDirectory = m_rootDirectory;
	Refresh();
}

void FileManager::SetCurrentDirectory(const std::filesystem::path& currentDirectory)
{
	if (!m_fileSystem || currentDirectory.empty() || !m_fileSystem->Exists(currentDirectory) || !m_fileSystem->IsDirectory(currentDirectory))
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

	if (!m_fileSystem || m_currentDirectory.empty() || !m_fileSystem->Exists(m_currentDirectory) || !m_fileSystem->IsDirectory(m_currentDirectory))
	{
		return;
	}

	m_entries = m_fileSystem->ReadDirectory(m_currentDirectory);
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

	const std::filesystem::path absolutePath = m_fileSystem->Absolute(m_selectedPath);

	try
	{
		auto importedObject = std::make_unique<Entity>(absolutePath.string().c_str());
		importedObject->SetIgnoreCameraCollision(true);
		if (!Root::Current().Levels().ActiveLevel())
		{
			Root::Current().Levels().CreateLevel("Default");
		}
		Root::Current().Levels().ActiveLevel()->AddObject(std::move(importedObject));
		Root::Current().Debugger().LogMessage("Imported FBX: " + absolutePath.string());
		return true;
	}
	catch (const std::exception& ex)
	{
		Root::Current().Debugger().LogMessage("Failed to import FBX '" + absolutePath.string() + "': " + ex.what());
		return false;
	}
}

const std::vector<std::filesystem::directory_entry>& FileManager::Entries() const
{
	return m_entries;
}

void FileManager::ClearEntries()
{
	m_entries.clear();
}

