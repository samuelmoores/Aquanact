#pragma once

#include <filesystem>

class AquanactBuildSystem {
public:
	bool Build(const std::filesystem::path& sourceRoot,
		const std::filesystem::path& buildRoot,
		const std::filesystem::path& projectFile,
		const std::filesystem::path& executablePath) const;
};

