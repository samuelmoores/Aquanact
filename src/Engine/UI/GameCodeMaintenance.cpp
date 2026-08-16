#include "Engine/UI/GameCodeMaintenance.h"

#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Root.h"

#include <algorithm>
#include <system_error>
#include <vector>

namespace
{
	std::vector<std::filesystem::path> CollectGameSourceFiles(const std::filesystem::path& gameSourceRoot)
	{
		std::vector<std::filesystem::path> files;
		for (const auto& entry : Root::Current().FileSystemRef().ReadDirectory(gameSourceRoot))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".cpp")
			{
				files.push_back(entry.path());
			}
		}
		std::sort(files.begin(), files.end());
		files.erase(std::unique(files.begin(), files.end()), files.end());
		return files;
	}
}

std::filesystem::path GameCodeMaintenance::SourceRoot()
{
#ifdef AQUANACT_SOURCE_ROOT
	return std::filesystem::path(AQUANACT_SOURCE_ROOT);
#else
	return std::filesystem::current_path();
#endif
}

std::filesystem::path GameCodeMaintenance::GameIncludeRoot()
{
	return SourceRoot() / "include" / "Game";
}

std::filesystem::path GameCodeMaintenance::GameSourceRoot()
{
	return SourceRoot() / "src" / "Game";
}

std::filesystem::path GameCodeMaintenance::GeneratedRoot()
{
	return SourceRoot() / "generated";
}

std::filesystem::path GameCodeMaintenance::GameRegistryPath()
{
	return SourceRoot() / "src" / "Engine" / "Core" / "ComponentRegistry.cpp";
}

std::string GameCodeMaintenance::MakeHeaderTemplate(const std::string& className)
{
	return
		"#pragma once\n\n"
		"#include \"Engine/Core/Component.h\"\n\n"
		"class Entity;\n"
		"class EntityStateMachine;\n"
		"class Input;\n"
		"class InputManager;\n"
		"class Root;\n\n"
		"// Generated gameplay component scaffold.\n"
		"// Keep the binding lists as the single source of truth for exposed data.\n"
		"// Add bindable values and events here when the component needs editor metadata.\n"
		"class " + className + " final : public Component\n"
		"{\n"
		"public:\n"
		"\t" + className + "() = default;\n\n"
		"\tconst char* Name() const override { return \"" + className + "\"; }\n"
		"\tvoid startUp(Entity&) override;\n"
		"\tvoid Update(Entity&, float) override {}\n"
		"\tvoid FirstFrame(Entity&) override {}\n"
		"\n"
		"\t// Put the component's exposed value list here. This is the only place\n"
		"\t// that should enumerate values the editor needs to see.\n"
		"};\n";
}

std::string GameCodeMaintenance::MakeSourceTemplate(const std::string& className)
{
	return
		"#include \"Game/" + className + ".h\"\n\n"
		"#include \"Engine/Core/Entity.h\"\n"
		"#include \"Engine/Core/EntityStateMachine.h\"\n"
		"#include \"Engine/Core/Input.h\"\n"
		"#include \"Engine/Core/InputManager.h\"\n"
		"#include \"Engine/Core/Root.h\"\n\n"
		"void " + className + "::startUp(Entity&)\n"
		"{\n"
		"}\n\n"
		"// Keep component metadata in the header with the binding/event list macros.\n"
		"// Add runtime logic here only if the component needs it.\n";
}

std::string GameCodeMaintenance::MakeGameSourcesList()
{
	std::string result = "set(GAME_SOURCES\n";
	for (const auto& path : CollectGameSourceFiles(GameSourceRoot()))
	{
		result += "    \"${CMAKE_SOURCE_DIR}/src/Game/" + path.filename().string() + "\"\n";
	}
	result += "    \"${CMAKE_SOURCE_DIR}/src/Engine/Core/ComponentRegistry.cpp\"\n)\n";
	return result;
}

std::string GameCodeMaintenance::MakeComponentRegistryTemplate()
{
	std::string result =
		"#include \"Engine/Core/ComponentRegistry.h\"\n\n"
		"#include \"Engine/Core/ComponentFactory.h\"\n"
		"#include \"Engine/Core/Controller.h\"\n"
		"#include \"Engine/Core/Entity.h\"\n";
	std::vector<std::string> names;
	for (const auto& path : CollectGameSourceFiles(GameSourceRoot()))
	{
		if (path.stem() != "ComponentRegistry") names.push_back(path.stem().string());
	}
	for (const std::string& name : names) result += "#include \"Game/" + name + ".h\"\n";
	result += "\n#include <memory>\n\nvoid RegisterGameComponents()\n{\n";
	result += "\tComponentFactory::Instance().Register(\"Controller\", [](Entity&) -> std::unique_ptr<Component>\n\t{\n\t\treturn std::make_unique<Controller>();\n\t});\n";
	for (const std::string& name : names)
		result += "\tComponentFactory::Instance().Register(\"" + name + "\", [](Entity&) -> std::unique_ptr<Component>\n\t{\n\t\treturn std::make_unique<" + name + ">();\n\t});\n";
	return result + "}\n";
}

GameCodeGenerationResult GameCodeMaintenance::CreateComponent(const std::string& className)
{
	const auto headerPath = GameIncludeRoot() / (className + ".h");
	const auto sourcePath = GameSourceRoot() / (className + ".cpp");
	const auto generatedPath = GeneratedRoot() / "GameSources.cmake";
	const auto registryPath = GameRegistryPath();
	std::error_code error;
	std::filesystem::create_directories(headerPath.parent_path(), error);
	std::filesystem::create_directories(sourcePath.parent_path(), error);
	const bool headerWritten = Root::Current().FileSystemRef().WriteTextFile(headerPath, MakeHeaderTemplate(className));
	const bool sourceWritten = Root::Current().FileSystemRef().WriteTextFile(sourcePath, MakeSourceTemplate(className));
	bool listWritten = false;
	bool registryWritten = false;
	if (headerWritten && sourceWritten)
	{
		std::filesystem::create_directories(GeneratedRoot(), error);
		listWritten = Root::Current().FileSystemRef().WriteTextFile(generatedPath, MakeGameSourcesList());
		registryWritten = Root::Current().FileSystemRef().WriteTextFile(registryPath, MakeComponentRegistryTemplate());
	}
	return {
		headerWritten && sourceWritten && listWritten && registryWritten,
		headerWritten && sourceWritten && listWritten && registryWritten
			? "Created " + headerPath.string() + ", " + sourcePath.string() + ", " + registryPath.string() + " and updated " + generatedPath.string()
			: "Failed to create one or more files."
	};
}

bool GameCodeMaintenance::RegenerateBuildFiles()
{
	std::error_code error;
	std::filesystem::create_directories(GeneratedRoot(), error);
	const bool listWritten = Root::Current().FileSystemRef().WriteTextFile(GeneratedRoot() / "GameSources.cmake", MakeGameSourcesList());
	const bool registryWritten = Root::Current().FileSystemRef().WriteTextFile(GameRegistryPath(), MakeComponentRegistryTemplate());
	return listWritten && registryWritten;
}

bool GameCodeMaintenance::DeleteComponentFiles(const std::string& componentName)
{
	const auto headerPath = GameIncludeRoot() / (componentName + ".h");
	const auto sourcePath = GameSourceRoot() / (componentName + ".cpp");
	bool deletedAny = false;
	if (std::filesystem::exists(headerPath)) deletedAny |= std::filesystem::remove(headerPath);
	if (std::filesystem::exists(sourcePath)) deletedAny |= std::filesystem::remove(sourcePath);
	return deletedAny;
}
