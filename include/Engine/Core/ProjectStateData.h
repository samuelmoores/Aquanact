#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace ProjectStateData {
	struct PendingController {
		std::filesystem::path sourcePath;
		unsigned int entityId = 0;
		float moveSpeed = 50.0f;
		float movementDeadzone = 0.01f;
		float turnSpeed = 8.0f;
		float groundAcceleration = 4000.0f;
		float airAcceleration = 800.0f;
		float groundFriction = 5000.0f;
		float airDrag = 0.1f;
		std::string levelName;
		bool playerControlled = false;
	};

	struct PendingComponent {
		std::filesystem::path sourcePath;
		unsigned int entityId = 0;
		std::string levelName;
		std::string type;
		std::string initialState;
		struct AnimatorStateData {
			std::string name;
			int clipIndex = -1;
		};
		struct AnimatorConditionData {
			struct OperandData {
				int type = 0;
				float constantValue = 0.0f;
				std::string componentName;
				std::string memberName;
			};
			OperandData left;
			int comparator = 0;
			OperandData right;
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
			std::vector<AnimatorConditionData> conditions;
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
			unsigned int id = 0;
			bool ignoreCameraCollision = false;
			bool showPhysicsBoundingBox = false;
			int physicsColliderShape = 0;
		};
		std::vector<PendingObject> objects;
	};

	struct RenderStateData {
		glm::vec3 gameCameraPosition{ 0.0f };
		glm::vec3 gameCameraFacing{ 0.0f };
		unsigned int gameCameraTargetId = 0;
		std::string gameCameraTarget;
		float gameCameraRadius = 10.0f;
		float gameCameraYaw = 0.0f;
		float gameCameraPitch = 15.0f;
		float gameCameraColliderRadius = 25.0f;
		bool gameCameraHasTarget = false;
		bool editorShowAxis = true;
		bool editorShowGrid = true;
		bool debugShowLogWindow = false;
		bool debugShowStatsWindow = false;
		bool showFileExplorer = false;
		bool showLevelWindow = true;
		bool showEntityWindow = false;
		bool showLightingWindow = false;
		bool showInputMapWindow = false;
		bool showCameraWindow = false;
		bool showGameInputWindow = true;
		bool showGameplayDiagnosticsWindow = true;
		bool showAnimationDiagnosticsWindow = true;
		bool showGameGUIDiagnosticsWindow = true;
		bool profilerEnabled = false;
		bool showCameraCollisionDebug = false;
		bool showPhysicsDiagnosticsWindow = true;
		struct DirectionalLightData {
			glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
			glm::vec3 color{ 1.0f };
			float intensity = 1.0f;
			float ambient = 0.2f;
			bool shadowsEnabled = true;
			bool castsShadows = true;
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
			bool castsShadows = false;
		};
		DirectionalLightData sunLight;
		std::vector<PointLightData> pointLights;
		std::string imguiLayout;
	};
}

