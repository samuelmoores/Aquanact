#pragma once

#include <filesystem>
#include <string>

class AquanactBuildSystem {
public:
	struct Result
	{
		bool succeeded = false;
		std::string message;
	};

	Result Build(const std::filesystem::path& sourceRoot,
		const std::filesystem::path& buildRoot,
		const std::filesystem::path& projectFile,
		const std::filesystem::path& executablePath,
		const std::filesystem::path& dependencyDirectory = {}) const;
};
