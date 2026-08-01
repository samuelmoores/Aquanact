#include "Engine/Core/AquanactBuildSystem.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
	const std::filesystem::path sourceRoot = argc > 1 ? argv[1] : std::filesystem::path("C:/dev/Aquanact");
	const std::filesystem::path buildRoot = argc > 2 ? argv[2] : std::filesystem::path("C:/dev/Aquanact/out/package");
	const std::filesystem::path projectFile = argc > 3 ? argv[3] : sourceRoot / "assets/projects/project.aqua";
	const std::filesystem::path executablePath = argc > 4 ? argv[4] : sourceRoot / "out/build/x64-debug/AquanactGame.exe";

	AquanactBuildSystem buildSystem;
	if (!buildSystem.Build(sourceRoot, buildRoot, projectFile, executablePath))
	{
		std::cerr << "AquanactBuildSystem: build failed\n";
		return 1;
	}

	std::cout << "AquanactBuildSystem: build succeeded\n";
	return 0;
}

