#pragma once

#include <filesystem>
#include <string>
#include <vector>

class LevelManager;

namespace ProjectStateFormat {
	std::string EscapeField(const std::string& value);
	std::string UnescapeField(const std::string& value);
	std::vector<std::string> SplitFields(const std::string& line);
	std::string HexEncode(const char* data, std::size_t size);
	std::string HexDecode(const std::string& text);
	std::filesystem::path MakePortableSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath);
	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath);
	void AppendLevelState(std::string& contents, const std::filesystem::path& projectPath, const LevelManager& levelManager);
}

