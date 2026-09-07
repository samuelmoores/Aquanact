#include "Engine/Core/AquanactBuildSystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
	bool IsWithin(const std::filesystem::path& child, const std::filesystem::path& parent)
	{
		const std::string childText = child.lexically_normal().generic_string();
		const std::string parentText = parent.lexically_normal().generic_string();
		return childText == parentText ||
			(childText.size() > parentText.size() && childText.rfind(parentText + "/", 0) == 0);
	}

	void CopyDirectory(const std::filesystem::path& source, const std::filesystem::path& destination)
	{
		std::filesystem::create_directories(destination);
		std::filesystem::copy(source, destination,
			std::filesystem::copy_options::recursive |
			std::filesystem::copy_options::overwrite_existing);
	}

	std::vector<std::string> CopyDllsFromDirectory(const std::filesystem::path& sourceDir,
		const std::filesystem::path& destinationDir)
	{
		std::vector<std::string> copiedDlls;
		if (sourceDir.empty() || !std::filesystem::is_directory(sourceDir))
			return copiedDlls;

		std::filesystem::create_directories(destinationDir);
		for (const auto& entry : std::filesystem::directory_iterator(sourceDir))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".dll")
				continue;

			const std::filesystem::path destination = destinationDir / entry.path().filename();
			if (std::filesystem::exists(destination))
				throw std::runtime_error("Duplicate runtime DLL: " + entry.path().filename().string());

			std::filesystem::copy_file(entry.path(), destination);
			copiedDlls.push_back(entry.path().filename().string());
		}
		return copiedDlls;
	}
}

AquanactBuildSystem::Result AquanactBuildSystem::Build(
	const std::filesystem::path& sourceRoot,
	const std::filesystem::path& buildRoot,
	const std::filesystem::path& projectFile,
	const std::filesystem::path& executablePath,
	const std::filesystem::path& dependencyDirectory) const
{
	Result result;
	std::filesystem::path stagingRoot;
	std::filesystem::path backupRoot;
	std::filesystem::path output;

	try
	{
		const auto absolute = [](const std::filesystem::path& path)
		{
			return std::filesystem::absolute(path).lexically_normal();
		};

		const std::filesystem::path source = absolute(sourceRoot);
		output = absolute(buildRoot);
		const std::filesystem::path project = absolute(projectFile);
		const std::filesystem::path executable = absolute(executablePath);

		if (!std::filesystem::is_directory(source))
			throw std::runtime_error("Source root does not exist: " + source.string());
		if (output == source || IsWithin(output, source))
			throw std::runtime_error("Output directory cannot be the source directory or one of its children: " + output.string());
		if (!std::filesystem::is_regular_file(project))
			throw std::runtime_error("Project file does not exist: " + project.string());
		const std::filesystem::path projectAssets = project.parent_path() / "assets";
		if (!std::filesystem::is_regular_file(executable))
			throw std::runtime_error("Game executable does not exist: " + executable.string());
		if (!std::filesystem::is_directory(source / "resources"))
			throw std::runtime_error("Engine resources directory does not exist: " + (source / "resources").string());
		if (!std::filesystem::is_directory(source / "shaders"))
			throw std::runtime_error("Shaders directory does not exist: " + (source / "shaders").string());

		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		stagingRoot = output.parent_path() /
			(output.filename().string() + ".staging-" + std::to_string(stamp));
		std::filesystem::create_directories(stagingRoot);

		CopyDirectory(source / "shaders", stagingRoot / "shaders");
		CopyDirectory(source / "resources", stagingRoot / "resources");
		// Overlay the selected project's assets on top of engine runtime assets.
		// Project-relative references therefore resolve inside the packaged game.
		if (std::filesystem::is_directory(projectAssets))
			CopyDirectory(projectAssets, stagingRoot / "assets");
		const std::filesystem::path myGuiMedia =
			source / "external" / "mygui-upstream" / "Media" / "MyGUI_Media";
		if (!std::filesystem::is_directory(myGuiMedia))
			throw std::runtime_error("MyGUI media directory does not exist: " + myGuiMedia.string());
		CopyDirectory(myGuiMedia, stagingRoot / "resources");
		const std::vector<std::string> requiredMyGuiResources = {
			"MyGUI_Core.xml",
			"MyGUI_CommonSkins.xml",
			"MyGUI_BlueWhiteSkins.xml",
			"MyGUI_BlueWhiteTemplates.xml",
			"MyGUI_BlueWhiteImages.xml",
			"MyGUI_Layers.xml",
			"MyGUI_Fonts.xml",
			"MyGUI_BlueWhiteSkins.png"
		};
		for (const std::string& resource : requiredMyGuiResources)
		{
			if (!std::filesystem::is_regular_file(stagingRoot / "resources" / resource))
				throw std::runtime_error("Required MyGUI resource was not staged: " + resource);
		}

		std::filesystem::copy_file(project, stagingRoot / "project.aqua");
		std::filesystem::copy_file(executable, stagingRoot / "game.exe");
		std::vector<std::string> runtimeDlls =
			CopyDllsFromDirectory(executable.parent_path(), stagingRoot);
		if (!dependencyDirectory.empty())
		{
			const std::vector<std::string> dependencyDlls =
				CopyDllsFromDirectory(absolute(dependencyDirectory), stagingRoot);
			runtimeDlls.insert(runtimeDlls.end(), dependencyDlls.begin(), dependencyDlls.end());
		}

		std::ofstream manifest(stagingRoot / "build.manifest", std::ios::trunc);
		if (!manifest.is_open())
			throw std::runtime_error("Could not write build manifest");
		manifest << "AquanactBuild 1\n";
		// Keep the package manifest relocatable too. Absolute source/build paths
		// belong in the editor log, not in files shipped with the game.
		manifest << "sourceRoot=.\n";
		manifest << "projectFile=project.aqua\n";
		manifest << "executablePath=game.exe\n";
		manifest << "runtimeDllCount=" << runtimeDlls.size() << "\n";
		for (const std::string& runtimeDll : runtimeDlls)
			manifest << "runtimeDll=" << runtimeDll << "\n";
		manifest.close();

		if (!std::filesystem::is_regular_file(stagingRoot / "game.exe") ||
			!std::filesystem::is_regular_file(stagingRoot / "project.aqua"))
			throw std::runtime_error("Staged package is incomplete");

		if (std::filesystem::exists(output))
		{
			const auto backupStamp = std::chrono::steady_clock::now().time_since_epoch().count();
			backupRoot = output.parent_path() /
				(output.filename().string() + ".previous-" + std::to_string(backupStamp));
			std::filesystem::rename(output, backupRoot);
		}
		std::filesystem::rename(stagingRoot, output);
		stagingRoot.clear();
		if (!backupRoot.empty())
		{
			std::error_code error;
			std::filesystem::remove_all(backupRoot, error);
			backupRoot.clear();
		}
		result.succeeded = true;
		result.message = "Build succeeded: " + output.string();
	}
	catch (const std::exception& exception)
	{
		result.message = exception.what();
		if (!backupRoot.empty() && !std::filesystem::exists(output))
		{
			std::error_code error;
			std::filesystem::rename(backupRoot, output, error);
		}
	}

	if (!stagingRoot.empty())
	{
		std::error_code error;
		std::filesystem::remove_all(stagingRoot, error);
	}
	return result;
}
