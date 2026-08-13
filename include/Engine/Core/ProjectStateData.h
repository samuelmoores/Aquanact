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
		float turnSpeed = 8.0f;
		std::string levelName;
		bool playerControlled = false;
	};

	struct PendingComponent {
		std::filesystem::path sourcePath;
		unsigned int entityId = 0;
		std::string levelName;
		std::string type;
		std::string componentClassName;
		std::string initialState;
		struct EntityStateData {
			std::string name;
			std::string animationName;
			bool blocksMovement = false;
			bool blocksInput = false;
		};
		struct EntityStateConditionData {
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
			struct EntityStateTransitionData {
				struct OperandData {
					int type = 0;
					float constantValue = 0.0f;
					std::string componentName;
					std::string memberName;
				};
				std::string from;
				std::string to;
				float blendSeconds = 0.33f;
				bool waitForCurrentStateComplete = false;
				OperandData left;
				int comparator = 0;
				OperandData right;
				std::vector<EntityStateConditionData> conditions;
			};
		std::vector<EntityStateData> entityStateStates;
		std::vector<EntityStateTransitionData> entityStateTransitions;
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
			bool blocksCameraView = true;
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

	struct PendingInputAction {
		std::string name;
		struct InputBindingData {
			int type = 0;
			int code = 0;
			int joystick = 0;
			float scale = 1.0f;
			glm::vec2 vector{ 0.0f };
			int stick = 0;
		};
		std::vector<InputBindingData> bindings;
	};
}


