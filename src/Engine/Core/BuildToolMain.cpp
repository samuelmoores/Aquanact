#include "Engine/Core/AquanactBuildSystem.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
	const std::filesystem::path sourceRoot = argc > 1 ? argv[1] : std::filesystem::current_path();
	const std::filesystem::path buildRoot = argc > 2 ? argv[2] : sourceRoot.parent_path() / "Aquanact-package";
	const std::filesystem::path projectFile = argc > 3 ? argv[3] : sourceRoot / "assets/projects/project.aqua";
	const std::filesystem::path executablePath = argc > 4 ? argv[4] : sourceRoot / "out/build/x64-debug/AquanactGame.exe";
	const std::filesystem::path dependencyDirectory = argc > 5 ? argv[5] : std::filesystem::path{};

	AquanactBuildSystem buildSystem;
	const AquanactBuildSystem::Result result = buildSystem.Build(
		sourceRoot, buildRoot, projectFile, executablePath, dependencyDirectory);
	if (!result.succeeded)
	{
		std::cerr << "AquanactBuildSystem: build failed: " << result.message << "\n";
		return 1;
	}

	std::cout << result.message << "\n";
	return 0;
}

