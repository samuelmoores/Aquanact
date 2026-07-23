#include "Engine/FileSystem.h"

#include <fstream>
#include <iterator>

#ifdef _WIN32
#include <windows.h>
#endif

std::filesystem::path FileSystem::ExecutableDirectory() const
{
#ifdef _WIN32
	std::wstring buffer;
	buffer.resize(MAX_PATH);

	for (;;)
	{
		const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (length == 0)
		{
			break;
		}
		if (length < buffer.size() - 1)
		{
			buffer.resize(length);
			return std::filesystem::path(buffer).parent_path();
		}
		buffer.resize(buffer.size() * 2);
	}
#endif
	return std::filesystem::current_path();
}

bool FileSystem::Exists(const std::filesystem::path& path) const
{
	return std::filesystem::exists(path);
}

bool FileSystem::IsDirectory(const std::filesystem::path& path) const
{
	return std::filesystem::is_directory(path);
}

std::filesystem::path FileSystem::Path(const char* path) const
{
	return std::filesystem::path(path);
}

std::filesystem::path FileSystem::Absolute(const std::filesystem::path& path) const
{
	return std::filesystem::absolute(path);
}

std::filesystem::path FileSystem::Relative(const std::filesystem::path& path, const std::filesystem::path& base, std::error_code& ec) const
{
	return std::filesystem::relative(path, base, ec);
}

std::vector<std::filesystem::directory_entry> FileSystem::ReadDirectory(const std::filesystem::path& path) const
{
	std::vector<std::filesystem::directory_entry> entries;
	if (!Exists(path) || !IsDirectory(path))
	{
		return entries;
	}

	for (const auto& entry : std::filesystem::directory_iterator(path))
	{
		entries.push_back(entry);
	}

	return entries;
}

std::vector<std::filesystem::directory_entry> FileSystem::ReadDirectoryRecursive(const std::filesystem::path& path) const
{
	std::vector<std::filesystem::directory_entry> entries;
	if (!Exists(path) || !IsDirectory(path))
	{
		return entries;
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
	{
		entries.push_back(entry);
	}

	return entries;
}

std::string FileSystem::ReadTextFile(const std::filesystem::path& path) const
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		return {};
	}

	return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool FileSystem::WriteTextFile(const std::filesystem::path& path, const std::string& contents) const
{
	std::ofstream file(path);
	if (!file.is_open())
	{
		return false;
	}

	file << contents;
	return true;
}

