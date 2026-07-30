#pragma once

#include "Engine/ProjectStateData.h"

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

class LevelManager;
class FrontEndManager;
class RenderManager;
class Entity;

namespace ProjectStateSerializer {
	std::string EscapeField(const std::string& value);
	std::string UnescapeField(const std::string& value);
	std::vector<std::string> SplitFields(const std::string& line);
	std::string HexEncode(const char* data, std::size_t size);
	std::string HexDecode(const std::string& text);
	std::filesystem::path MakePortableSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath);
	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath);
	void AppendLevelState(std::string& contents, const std::filesystem::path& projectPath, const LevelManager& levelManager);
	bool LoadLevelState(
		const std::filesystem::path& projectPath,
		std::istream& file,
		int projectVersion,
		std::vector<ProjectStateData::PendingLevel>& pendingLevels,
		std::vector<ProjectStateData::PendingController>& pendingControllers,
		std::vector<ProjectStateData::PendingComponent>& pendingComponents,
		std::vector<std::string>& pendingGameGUIAssets,
		std::string& pendingActiveGameGUIAsset,
		ProjectStateData::RenderStateData& renderState,
		std::string& startupLevelName);
	void AppendRenderState(std::string& contents, const FrontEndManager& frontEndManager, const RenderManager& renderManager);
}
