#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ProjectStateData {
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
		bool isCutscene = false;
		bool isMainMenu = false;
		struct PendingObject {
			std::filesystem::path sourcePath;
			glm::vec3 position{ 0.0f };
			glm::vec3 rotation{ 0.0f };
			glm::vec3 scale{ 1.0f };
			bool ignoreCameraCollision = true;
		};
		std::vector<PendingObject> objects;
	};

	struct RenderStateData {
		glm::vec3 gameCameraPosition{ 0.0f };
		glm::vec3 gameCameraFacing{ 0.0f };
		bool editorShowAxis = true;
		bool editorShowGrid = true;
		bool debugShowLogWindow = false;
		bool debugShowStatsWindow = false;
		struct DirectionalLightData {
			glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
			glm::vec3 color{ 1.0f };
			float intensity = 1.0f;
			float ambient = 0.2f;
		};
		struct PointLightData {
			glm::vec3 position{ 0.0f };
			glm::vec3 color{ 1.0f };
			float intensity = 1.0f;
			float ambient = 0.2f;
			float radius = 1.0f;
			float radiusFade = 1.0f;
			float constant = 1.0f;
			float linear = 0.0f;
			float quadratic = 0.0f;
		};
		DirectionalLightData sunLight;
		std::vector<PointLightData> pointLights;
		std::string imguiLayout;
	};
}

