#pragma once

#include <filesystem>
#include <string>
#include <vector>

class FileSystem {
public:
	FileSystem() = default;

	bool Exists(const std::filesystem::path& path) const;
	bool IsDirectory(const std::filesystem::path& path) const;
	std::filesystem::path Path(const char* path) const;
	std::filesystem::path ExecutableDirectory() const;
	std::filesystem::path Absolute(const std::filesystem::path& path) const;
	std::filesystem::path Relative(const std::filesystem::path& path, const std::filesystem::path& base, std::error_code& ec) const;
	std::vector<std::filesystem::directory_entry> ReadDirectory(const std::filesystem::path& path) const;
	std::vector<std::filesystem::directory_entry> ReadDirectoryRecursive(const std::filesystem::path& path) const;

	std::string ReadTextFile(const std::filesystem::path& path) const;
	bool WriteTextFile(const std::filesystem::path& path, const std::string& contents) const;
};
