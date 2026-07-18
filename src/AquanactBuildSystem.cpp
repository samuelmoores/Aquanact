#include "AquanactBuildSystem.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
	void CopyDirectoryIfExists(const std::filesystem::path& source, const std::filesystem::path& destination)
	{
		if (!std::filesystem::exists(source))
		{
			return;
		}

		std::filesystem::create_directories(destination);
		std::filesystem::copy(source, destination,
			std::filesystem::copy_options::recursive |
			std::filesystem::copy_options::overwrite_existing);
	}

	void CopyDllsFromDirectory(const std::filesystem::path& sourceDir, const std::filesystem::path& destinationDir)
	{
		if (!std::filesystem::exists(sourceDir))
		{
			return;
		}

		std::filesystem::create_directories(destinationDir);
		for (const auto& entry : std::filesystem::directory_iterator(sourceDir))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".dll")
			{
				continue;
			}

			std::filesystem::copy_file(
				entry.path(),
				destinationDir / entry.path().filename(),
				std::filesystem::copy_options::overwrite_existing);
		}
	}
}

bool AquanactBuildSystem::Build(const std::filesystem::path& sourceRoot,
	const std::filesystem::path& buildRoot,
	const std::filesystem::path& projectFile,
	const std::filesystem::path& executablePath) const
{
	try
	{
		std::filesystem::create_directories(buildRoot);

		CopyDirectoryIfExists(sourceRoot / "shaders", buildRoot / "shaders");
		CopyDirectoryIfExists(sourceRoot / "assets", buildRoot / "assets");
		CopyDirectoryIfExists(sourceRoot / "external" / "mygui-upstream" / "Media" / "MyGUI_Media", buildRoot);

		if (std::filesystem::exists(projectFile))
		{
			std::filesystem::copy_file(
				projectFile,
				buildRoot / projectFile.filename(),
				std::filesystem::copy_options::overwrite_existing);
		}

		if (std::filesystem::exists(executablePath))
		{
			std::filesystem::copy_file(
				executablePath,
				buildRoot / "game.exe",
				std::filesystem::copy_options::overwrite_existing);
		}

		CopyDllsFromDirectory(executablePath.parent_path(), buildRoot);
		CopyDllsFromDirectory(sourceRoot / "out" / "build" / "x64-debug" / "vcpkg_installed" / "x64-windows" / "debug" / "bin", buildRoot);
		CopyDllsFromDirectory(sourceRoot / "out" / "build" / "x64-debug" / "vcpkg_installed" / "x64-windows" / "bin", buildRoot);

		std::ofstream manifest(buildRoot / "build.manifest", std::ios::trunc);
		if (!manifest.is_open())
		{
			return false;
		}

		manifest << "AquanactBuild 1\n";
		manifest << "sourceRoot=" << sourceRoot.string() << "\n";
		manifest << "projectFile=" << projectFile.string() << "\n";
		manifest << "executablePath=" << executablePath.string() << "\n";
		return true;
	}
	catch (...)
	{
		return false;
	}
}
