#pragma once

#include <filesystem>
#include <string>

struct GameCodeGenerationResult
{
	bool success = false;
	std::string statusMessage;
};

class GameCodeMaintenance
{
public:
	static GameCodeGenerationResult CreateComponent(const std::string& className);
	static bool DeleteComponentFiles(const std::string& componentName);
	static bool RegenerateBuildFiles();

private:
	static std::filesystem::path SourceRoot();
	static std::filesystem::path GameIncludeRoot();
	static std::filesystem::path GameSourceRoot();
	static std::filesystem::path GeneratedRoot();
	static std::filesystem::path GameRegistryPath();
	static std::string MakeHeaderTemplate(const std::string& className);
	static std::string MakeSourceTemplate(const std::string& className);
	static std::string MakeGameSourcesList();
	static std::string MakeComponentRegistryTemplate();
};
