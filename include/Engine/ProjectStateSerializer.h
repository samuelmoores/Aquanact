#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

class LevelManager;
class FrontEndManager;
class RenderManager;
class Entity;

namespace ProjectStateSerializer {
	struct PendingController {
		std::filesystem::path sourcePath;
		float moveSpeed = 50.0f;
		std::string levelName;
		bool playerControlled = false;
	};

	struct PendingComponent {
		std::filesystem::path sourcePath;
		std::string levelName;
		std::string type;
		std::string initialState;
		struct AnimatorStateData {
			std::string name;
			int clipIndex = -1;
		};
		struct AnimatorTransitionData {
			struct OperandData {
				int type = 0;
				float constantValue = 0.0f;
				std::string componentName;
				std::string memberName;
			};
			std::string from;
			std::string to;
			float blendSeconds = 0.33f;
			OperandData left;
			int comparator = 0;
			OperandData right;
		};
		std::vector<AnimatorStateData> animatorStates;
		std::vector<AnimatorTransitionData> animatorTransitions;
		int value1 = 0;
		int value2 = 0;
		bool hasValue1 = false;
		bool hasValue2 = false;
	};

	struct PendingLevel {
		std::string name;
		bool active = false;
		std::vector<std::unique_ptr<Entity>> objects;
	};

	std::string EscapeField(const std::string& value);
	std::string UnescapeField(const std::string& value);
	std::vector<std::string> SplitFields(const std::string& line);
	std::string HexEncode(const char* data, std::size_t size);
	std::string HexDecode(const std::string& text);
	std::filesystem::path MakePortableSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath);
	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath);
	void AppendLevelState(std::string& contents, const std::filesystem::path& projectPath, const LevelManager& levelManager);
	bool LoadLevelState(const std::filesystem::path& projectPath, std::istream& file, int projectVersion, LevelManager& levelManager, FrontEndManager& frontEndManager, RenderManager& renderManager, std::vector<std::string>& pendingGameGUIAssets, std::string& pendingActiveGameGUIAsset, std::string& pendingImguiLayout);
	void AppendRenderState(std::string& contents, const FrontEndManager& frontEndManager, const RenderManager& renderManager);
	void ApplyRenderState(const std::vector<std::string>& fields, FrontEndManager& frontEndManager, RenderManager& renderManager, std::string& pendingImguiLayout);
}
