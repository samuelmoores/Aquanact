#include "Engine/Core/ProjectStateSerializer.h"
#include "Engine/Core/ProjectStateFormat.h"

#include "Engine/Core/AnimatorComponent.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/EngineGUI.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/PlayerController.h"
#include "Engine/Core/RenderManager.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <glm/glm.hpp>
#include <istream>
#include <sstream>

namespace ProjectStateSerializer {
	using ProjectStateData::PendingComponent;
	using ProjectStateData::PendingController;
	using ProjectStateData::PendingLevel;
	using ProjectStateData::RenderStateData;

	namespace {
		void AppendComponentLine(std::string& contents, const std::filesystem::path& projectPath, const Entity* object, const char* componentType)
		{
			contents += "component;";
			contents += ProjectStateFormat::EscapeField(ProjectStateFormat::MakePortableSourcePath(projectPath, object->SourcePath()).string());
			contents += ";";
			contents += std::to_string(object->Id());
			contents += ";";
			contents += componentType;
		}

		bool IsComponentType(const std::string& type)
		{
			return type == "controller" || type == "playercontroller" || type == "playerhealth" || type == "enemy" || type == "animator";
		}

		struct ComponentRecordLayout
		{
			std::size_t typeIndex = 2;
			std::size_t dataIndex = 3;
			unsigned int entityId = 0;
		};

		bool ReadComponentRecordLayout(const std::vector<std::string>& fields, ComponentRecordLayout& layout)
		{
			if (fields.size() < 3 || fields[0] != "component")
			{
				return false;
			}

			if (IsComponentType(fields[2]))
			{
				return true;
			}

			if (fields.size() < 4 || !IsComponentType(fields[3]))
			{
				return false;
			}

			try
			{
				layout.entityId = static_cast<unsigned int>(std::stoul(fields[2]));
			}
			catch (...)
			{
				return false;
			}
			layout.typeIndex = 3;
			layout.dataIndex = 4;
			return true;
		}

		void AppendComponentState(std::string& contents, const std::filesystem::path& projectPath, const Entity* object)
		{
			for (const Component* component : object->Components())
			{
				if (!component)
				{
					continue;
				}

				if (const PlayerController* playerController = dynamic_cast<const PlayerController*>(component))
				{
					AppendComponentLine(contents, projectPath, object, "playercontroller");
					contents += ";" + std::to_string(playerController->MoveSpeed());
					contents += ";" + std::to_string(playerController->TurnSpeed());
					contents += ";" + std::to_string(playerController->MovementDeadzone()) + "\n";
				}
				else if (const Controller* controller = dynamic_cast<const Controller*>(component))
				{
					AppendComponentLine(contents, projectPath, object, "controller");
					contents += ";" + std::to_string(controller->MoveSpeed());
					contents += ";" + std::to_string(controller->MovementDeadzone()) + "\n";
				}
				else if (const PlayerHealth* playerHealth = dynamic_cast<const PlayerHealth*>(component))
				{
					AppendComponentLine(contents, projectPath, object, "playerhealth");
					contents += ";" + std::to_string(playerHealth->Health());
					contents += ";" + std::to_string(playerHealth->MaxHealth()) + "\n";
				}
				else if (dynamic_cast<const Enemy*>(component))
				{
					AppendComponentLine(contents, projectPath, object, "enemy");
					contents += "\n";
				}
				else if (const AnimatorComponent* animator = dynamic_cast<const AnimatorComponent*>(component))
				{
					AppendComponentLine(contents, projectPath, object, "animator");
					contents += ";" + ProjectStateFormat::EscapeField(animator->InitialState());
					contents += ";" + std::to_string(animator->States().size());
					for (const auto& state : animator->States())
					{
						contents += ";" + ProjectStateFormat::EscapeField(state.name);
						contents += ";" + std::to_string(state.clipIndex);
					}

					contents += ";" + std::to_string(animator->Transitions().size());
					for (const auto& transition : animator->Transitions())
					{
						const auto appendOperand = [&contents](const AnimatorComponent::Operand& operand)
						{
							contents += ";" + std::to_string(static_cast<int>(operand.type));
							contents += ";" + std::to_string(operand.constantValue);
							contents += ";" + ProjectStateFormat::EscapeField(operand.componentName);
							contents += ";" + ProjectStateFormat::EscapeField(operand.memberName);
						};

						contents += ";" + ProjectStateFormat::EscapeField(transition.from);
						contents += ";" + ProjectStateFormat::EscapeField(transition.to);
					contents += ";" + std::to_string(transition.blendSeconds);
					const auto& conditions = transition.conditions.empty()
						? std::vector<AnimatorComponent::Condition>{ transition.condition }
						: transition.conditions;
					contents += ";" + std::to_string(conditions.size());
					for (const auto& condition : conditions)
					{
						appendOperand(condition.left);
						contents += ";" + std::to_string(static_cast<int>(condition.comparator));
						appendOperand(condition.right);
					}
					}
					contents += "\n";
				}
			}
		}
	}

	std::string EscapeField(const std::string& value) { return ProjectStateFormat::EscapeField(value); }
	std::string UnescapeField(const std::string& value) { return ProjectStateFormat::UnescapeField(value); }
	std::vector<std::string> SplitFields(const std::string& line) { return ProjectStateFormat::SplitFields(line); }
	std::string HexEncode(const char* data, std::size_t size) { return ProjectStateFormat::HexEncode(data, size); }
	std::string HexDecode(const std::string& text) { return ProjectStateFormat::HexDecode(text); }
	std::filesystem::path MakePortableSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath) { return ProjectStateFormat::MakePortableSourcePath(projectPath, sourcePath); }
	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath) { return ProjectStateFormat::ResolveSourcePath(projectPath, sourcePath); }
	void AppendLevelState(std::string& contents, const std::filesystem::path& projectPath, const SceneManager& SceneManager)
	{
		ProjectStateFormat::AppendLevelState(contents, projectPath, SceneManager);

		// Transform data is written by ProjectStateFormat. Component behavior is
		// written here so the serializer owns the complete entity reconstruction
		// record for the component types currently supported by the engine.
		for (const auto& scene : SceneManager.Levels())
		{
			if (!scene)
			{
				continue;
			}
			contents += "scenecontext;" + ProjectStateFormat::EscapeField(scene->Name()) + "\n";
			for (const auto& object : scene->Objects())
			{
				if (object)
				{
					AppendComponentState(contents, projectPath, object.get());
				}
			}
		}
	}

	bool LoadLevelState(
		const std::filesystem::path& projectPath,
		std::istream& file,
		int projectVersion,
		std::vector<PendingLevel>& pendingLevels,
		std::vector<PendingController>& pendingControllers,
		std::vector<PendingComponent>& pendingComponents,
		std::vector<std::string>& pendingGameGUIAssets,
		std::string& pendingActiveGameGUIAsset,
		RenderStateData& renderState,
		std::string& startupLevelName)
	{
		PendingLevel* currentLevel = nullptr;
		std::string line;

		while (std::getline(file, line))
		{
			if (line.empty())
			{
				continue;
			}

			const std::vector<std::string> fields = ProjectStateFormat::SplitFields(line);
			if ((fields.size() >= 7 && fields[0] == "gamecamera") || (fields.size() == 3 && fields[0] == "editorview") || (fields.size() >= 3 && fields[0] == "debugwindows") || ((fields.size() >= 8 && fields.size() <= 11) && fields[0] == "sunlight") || ((fields.size() >= 12 && fields.size() <= 15) && fields[0] == "pointlight") || (fields.size() == 2 && fields[0] == "imguilayout"))
			{
				if (fields.size() >= 7 && fields[0] == "gamecamera")
				{
					if (fields.size() >= 7)
					{
						renderState.gameCameraPosition = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
						renderState.gameCameraFacing = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
					}
					if (fields.size() >= 8)
					{
						renderState.gameCameraRadius = std::stof(fields[7]);
					}
					if (fields.size() >= 9)
					{
						renderState.gameCameraYaw = std::stof(fields[8]);
					}
					if (fields.size() >= 10)
					{
						renderState.gameCameraPitch = std::stof(fields[9]);
					}
					if (fields.size() >= 11)
					{
						try
						{
							renderState.gameCameraTargetId = static_cast<unsigned int>(std::stoul(fields[10]));
							renderState.gameCameraHasTarget = renderState.gameCameraTargetId != 0;
						}
						catch (...)
						{
							renderState.gameCameraTargetId = 0;
							renderState.gameCameraHasTarget = false;
							Root::Current().Debugger().LogTagged(Debug::Severity::Warning, "ProjectLoad", "Failed to parse camera target id from gamecamera line");
						}
					}
					if (fields.size() >= 12)
					{
						renderState.gameCameraColliderRadius = std::stof(fields[11]);
					}
				}
				else if (fields.size() == 3 && fields[0] == "editorview")
				{
					renderState.editorShowAxis = fields[1] == "1" || fields[1] == "true" || fields[1] == "True";
					renderState.editorShowGrid = fields[2] == "1" || fields[2] == "true" || fields[2] == "True";
				}
				else if (fields.size() >= 3 && fields[0] == "debugwindows")
				{
					renderState.debugShowLogWindow = fields[1] == "1" || fields[1] == "true" || fields[1] == "True";
					renderState.debugShowStatsWindow = fields[2] == "1" || fields[2] == "true" || fields[2] == "True";
					const auto readBool = [&fields](std::size_t index, bool fallback)
					{
						if (index >= fields.size()) return fallback;
						return fields[index] == "1" || fields[index] == "true" || fields[index] == "True";
					};
					renderState.showFileExplorer = readBool(3, renderState.showFileExplorer);
					renderState.showLevelWindow = readBool(4, renderState.showLevelWindow);
					renderState.showEntityWindow = readBool(5, renderState.showEntityWindow);
					renderState.showLightingWindow = readBool(6, renderState.showLightingWindow);
					renderState.showInputMapWindow = readBool(7, renderState.showInputMapWindow);
					renderState.showCameraWindow = readBool(8, renderState.showCameraWindow);
					renderState.showGameInputWindow = readBool(9, renderState.showGameInputWindow);
					renderState.showGameplayDiagnosticsWindow = readBool(10, renderState.showGameplayDiagnosticsWindow);
					renderState.showAnimationDiagnosticsWindow = readBool(11, renderState.showAnimationDiagnosticsWindow);
						 renderState.showGameGUIDiagnosticsWindow = readBool(12, renderState.showGameGUIDiagnosticsWindow);
						renderState.profilerEnabled = readBool(13, renderState.profilerEnabled);
						renderState.showCameraCollisionDebug = readBool(14, renderState.showCameraCollisionDebug);
						renderState.showPhysicsDiagnosticsWindow = readBool(15, renderState.showPhysicsDiagnosticsWindow);
				}
				else if (fields.size() >= 8 && fields.size() <= 11 && fields[0] == "sunlight")
				{
					renderState.sunLight.direction = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
					renderState.sunLight.color = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
					renderState.sunLight.intensity = std::stof(fields[7]);
					if (fields.size() >= 9)
					{
						renderState.sunLight.ambient = std::stof(fields[8]);
					}
					if (fields.size() >= 10)
					{
						renderState.sunLight.shadowsEnabled = fields[9] == "1" || fields[9] == "true" || fields[9] == "True";
					}
					if (fields.size() >= 11)
					{
						renderState.sunLight.castsShadows = fields[10] == "1" || fields[10] == "true" || fields[10] == "True";
					}
				}
				else if (fields.size() >= 12 && fields.size() <= 15 && fields[0] == "pointlight")
				{
					RenderStateData::PointLightData pointLight;
					pointLight.position = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
					pointLight.color = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
					pointLight.intensity = std::stof(fields[7]);
					if (fields.size() >= 14)
					{
						pointLight.ambient = std::stof(fields[8]);
						pointLight.radius = std::stof(fields[9]);
						pointLight.radiusFade = std::stof(fields[10]);
						pointLight.constant = std::stof(fields[11]);
						pointLight.linear = std::stof(fields[12]);
						pointLight.quadratic = std::stof(fields[13]);
						if (fields.size() >= 15)
						{
							pointLight.castsShadows = fields[14] == "1" || fields[14] == "true" || fields[14] == "True";
						}
					}
					else
					{
						pointLight.radius = std::stof(fields[8]);
						if (fields.size() == 13)
						{
							pointLight.radiusFade = std::stof(fields[9]);
							pointLight.constant = std::stof(fields[10]);
							pointLight.linear = std::stof(fields[11]);
							pointLight.quadratic = std::stof(fields[12]);
						}
						else
						{
							pointLight.constant = std::stof(fields[9]);
							pointLight.linear = std::stof(fields[10]);
							pointLight.quadratic = std::stof(fields[11]);
						}
					}
					renderState.pointLights.push_back(std::move(pointLight));
				}
				else if (fields.size() == 2 && fields[0] == "imguilayout")
				{
					renderState.imguiLayout = ProjectStateFormat::HexDecode(fields[1]);
				}
				continue;
			}

			if (fields.size() == 2 && fields[0] == "startuplevel")
			{
				startupLevelName = ProjectStateFormat::UnescapeField(fields[1]);
				continue;
			}

			if (fields.size() == 2 && fields[0] == "scenecontext")
			{
				const std::string sceneName = ProjectStateFormat::UnescapeField(fields[1]);
				currentLevel = nullptr;
				for (auto& pendingLevel : pendingLevels)
				{
					if (pendingLevel.name == sceneName)
					{
						currentLevel = &pendingLevel;
						break;
					}
				}
				continue;
			}

			if (fields.size() == 2 && fields[0] == "gameguiasset")
			{
				pendingGameGUIAssets.push_back(ProjectStateFormat::UnescapeField(fields[1]));
				continue;
			}

			if (fields.size() == 2 && fields[0] == "gameguiactive")
			{
				pendingActiveGameGUIAsset = ProjectStateFormat::UnescapeField(fields[1]);
				continue;
			}

			ComponentRecordLayout componentLayout;
			if (ReadComponentRecordLayout(fields, componentLayout))
			{
				if (!currentLevel)
				{
					return false;
				}

				const std::string& componentType = fields[componentLayout.typeIndex];
				const std::filesystem::path sourcePath = ProjectStateFormat::ResolveSourcePath(projectPath, fields[1]);
				if (componentType == "controller" || componentType == "playercontroller")
				{
					if (fields.size() <= componentLayout.dataIndex)
					{
						return false;
					}

					PendingController controller;
					controller.sourcePath = sourcePath;
					controller.entityId = componentLayout.entityId;
					controller.moveSpeed = std::stof(fields[componentLayout.dataIndex]);
					controller.levelName = currentLevel->name;
					controller.playerControlled = componentType == "playercontroller" || projectVersion <= 11;
					if (componentLayout.entityId != 0)
					{
						if (controller.playerControlled && fields.size() > componentLayout.dataIndex + 1)
						{
							controller.turnSpeed = std::stof(fields[componentLayout.dataIndex + 1]);
						}
						const std::size_t deadzoneIndex = componentLayout.dataIndex + (controller.playerControlled ? 2 : 1);
						if (fields.size() > deadzoneIndex)
						{
							controller.movementDeadzone = std::stof(fields[deadzoneIndex]);
						}
					}
					pendingControllers.push_back(std::move(controller));
					continue;
				}

				PendingComponent component;
				component.sourcePath = sourcePath;
				component.entityId = componentLayout.entityId;
				component.levelName = currentLevel->name;
				component.type = componentType;

				if (componentType == "playerhealth")
				{
					if (fields.size() > componentLayout.dataIndex)
					{
						component.value1 = std::stoi(fields[componentLayout.dataIndex]);
						component.hasValue1 = true;
					}
					if (fields.size() > componentLayout.dataIndex + 1)
					{
						component.value2 = std::stoi(fields[componentLayout.dataIndex + 1]);
						component.hasValue2 = true;
					}
					pendingComponents.push_back(std::move(component));
					continue;
				}

				if (componentType == "enemy")
				{
					pendingComponents.push_back(std::move(component));
					continue;
				}

				if (componentType == "animator")
				{
					if (fields.size() < componentLayout.dataIndex + 3)
					{
						return false;
					}

					std::size_t index = componentLayout.dataIndex;
					component.initialState = ProjectStateFormat::UnescapeField(fields[index++]);
					const int stateCount = std::stoi(fields[index++]);
					for (int i = 0; i < stateCount; ++i)
					{
						PendingComponent::AnimatorStateData state;
						state.name = ProjectStateFormat::UnescapeField(fields[index++]);
						state.clipIndex = std::stoi(fields[index++]);
						component.animatorStates.push_back(std::move(state));
					}
					const int transitionCount = std::stoi(fields[index++]);
					for (int i = 0; i < transitionCount; ++i)
					{
						PendingComponent::AnimatorTransitionData transition;
						transition.from = ProjectStateFormat::UnescapeField(fields[index++]);
						transition.to = ProjectStateFormat::UnescapeField(fields[index++]);
						transition.blendSeconds = std::stof(fields[index++]);
						if (projectVersion >= 17)
						{
							const int conditionCount = std::stoi(fields[index++]);
							for (int conditionIndex = 0; conditionIndex < conditionCount; ++conditionIndex)
							{
								ProjectStateData::PendingComponent::AnimatorConditionData condition;
								condition.left.type = std::stoi(fields[index++]);
								condition.left.constantValue = std::stof(fields[index++]);
								condition.left.componentName = ProjectStateFormat::UnescapeField(fields[index++]);
								condition.left.memberName = ProjectStateFormat::UnescapeField(fields[index++]);
								condition.comparator = std::stoi(fields[index++]);
								condition.right.type = std::stoi(fields[index++]);
								condition.right.constantValue = std::stof(fields[index++]);
								condition.right.componentName = ProjectStateFormat::UnescapeField(fields[index++]);
								condition.right.memberName = ProjectStateFormat::UnescapeField(fields[index++]);
								transition.conditions.push_back(std::move(condition));
							}
						}
						else if (projectVersion >= 11)
						{
							transition.left.type = std::stoi(fields[index++]);
							transition.left.constantValue = std::stof(fields[index++]);
							transition.left.componentName = ProjectStateFormat::UnescapeField(fields[index++]);
							transition.left.memberName = ProjectStateFormat::UnescapeField(fields[index++]);
							transition.comparator = std::stoi(fields[index++]);
							transition.right.type = std::stoi(fields[index++]);
							transition.right.constantValue = std::stof(fields[index++]);
							transition.right.componentName = ProjectStateFormat::UnescapeField(fields[index++]);
							transition.right.memberName = ProjectStateFormat::UnescapeField(fields[index++]);
						}
						component.animatorTransitions.push_back(std::move(transition));
					}
					pendingComponents.push_back(std::move(component));
					continue;
				}
			}

			if (fields.size() >= 2 && fields[0] == "Scene")
			{
				PendingLevel Scene;
				Scene.name = ProjectStateFormat::UnescapeField(fields[1]);
				if (Scene.name.empty())
				{
					Scene.name = "Scene";
				}
				if (fields.size() >= 3)
				{
					Scene.active = fields[2] == "1" || fields[2] == "true" || fields[2] == "True";
				}
				if (fields.size() >= 4)
				{
					Scene.isCutscene = fields[3] == "cutscene";
				}
				if (fields.size() >= 5)
				{
					Scene.isMainMenu = fields[4] == "1" || fields[4] == "true" || fields[4] == "True";
				}
				pendingLevels.push_back(std::move(Scene));
				currentLevel = &pendingLevels.back();
				continue;
			}

			if (((fields.size() < 11 || fields.size() > 15) || fields[0] != "object"))
			{
				continue;
			}

			if (!currentLevel)
			{
				return false;
			}

			PendingLevel::PendingObject object;
			object.sourcePath = ProjectStateFormat::ResolveSourcePath(projectPath, fields[1]);
			object.position = glm::vec3(std::stof(fields[2]), std::stof(fields[3]), std::stof(fields[4]));
			object.rotation = glm::vec3(std::stof(fields[5]), std::stof(fields[6]), std::stof(fields[7]));
			object.scale = glm::vec3(std::stof(fields[8]), std::stof(fields[9]), std::stof(fields[10]));
			if (fields.size() >= 12)
			{
				try
				{
					object.id = static_cast<unsigned int>(std::stoul(fields[11]));
				}
				catch (...)
				{
					object.id = 0;
				}
			}
			if (fields.size() >= 13)
			{
				object.ignoreCameraCollision = fields[12] == "1" || fields[12] == "true" || fields[12] == "True";
			}
			if (fields.size() >= 14)
			{
				object.showPhysicsBoundingBox = fields[13] == "1" || fields[13] == "true" || fields[13] == "True";
			}
			if (fields.size() >= 15)
			{
				try
				{
					object.physicsColliderShape = std::stoi(fields[14]) == 1 ? 1 : 0;
				}
				catch (...)
				{
					object.physicsColliderShape = 0;
				}
			}
			currentLevel->objects.push_back(std::move(object));
		}

		return true;
	}

	void AppendRenderState(std::string& contents, const FrontEndManager& frontEndManager, const RenderManager& renderManager)
	{
		const glm::vec3 gameCameraPosition = renderManager.GetGameCamera().GetPosition();
		const glm::vec3 gameCameraFacing = renderManager.GetGameCamera().GetFacing();
		const GameCamera& gameCamera = renderManager.GetGameCamera();
		contents += "gamecamera;";
		contents += std::to_string(gameCameraPosition.x) + ";" + std::to_string(gameCameraPosition.y) + ";" + std::to_string(gameCameraPosition.z) + ";";
		contents += std::to_string(gameCameraFacing.x) + ";" + std::to_string(gameCameraFacing.y) + ";" + std::to_string(gameCameraFacing.z) + ";";
		contents += std::to_string(gameCamera.Radius()) + ";";
		contents += std::to_string(gameCamera.Yaw()) + ";";
		contents += std::to_string(gameCamera.Pitch()) + ";";
		contents += std::to_string(gameCamera.TargetId()) + ";";
		contents += std::to_string(gameCamera.ColliderRadius()) + "\n";

		contents += "editorview;";
		contents += frontEndManager.EditorGUI().ShowAxis() ? "1" : "0";
		contents += ";";
		contents += frontEndManager.EditorGUI().ShowGrid() ? "1" : "0";
		contents += "\n";
		contents += "debugwindows;";
		const bool showLogWindow = Root::Current().Debugger().ShowLogWindow();
		const bool showStatsWindow = Root::Current().Debugger().ShowStatsWindow();
		const bool showFileExplorer = Root::Current().FrontEnd().EditorGUI().ShowFileExplorer();
		const bool showLevelWindow = Root::Current().FrontEnd().EditorGUI().ShowLevelWindow();
		const bool showEntityWindow = Root::Current().FrontEnd().EditorGUI().ShowEntityWindow();
		const bool showLightingWindow = Root::Current().FrontEnd().EditorGUI().ShowLightingWindow();
		const bool showInputMapWindow = Root::Current().FrontEnd().EditorGUI().ShowInputMapWindow();
		const bool showCameraWindow = Root::Current().FrontEnd().EditorGUI().ShowCameraWindow();
		const bool showGameInputWindow = Root::Current().Debugger().ShowGameInputWindow();
		const bool showGameplayDiagnosticsWindow = Root::Current().Debugger().ShowGameplayDiagnosticsWindow();
		const bool showAnimationDiagnosticsWindow = Root::Current().Debugger().ShowAnimationDiagnosticsWindow();
		const bool showGameGUIDiagnosticsWindow = Root::Current().FrontEnd().RuntimeGUI().ShowDiagnosticsWindow();
		const bool profilerEnabled = Root::Current().Profiler().IsEnabled();
		const bool showCameraCollisionDebug = Root::Current().Debugger().ShowCameraCollisionDebug();
		const bool showPhysicsDiagnosticsWindow = Root::Current().Debugger().ShowPhysicsDiagnosticsWindow();
		contents += showLogWindow ? "1" : "0";
		contents += ";";
		contents += showStatsWindow ? "1" : "0";
		contents += ";";
		contents += showFileExplorer ? "1" : "0";
		contents += ";";
		contents += showLevelWindow ? "1" : "0";
		contents += ";";
		contents += showEntityWindow ? "1" : "0";
		contents += ";";
		contents += showLightingWindow ? "1" : "0";
		contents += ";";
		contents += showInputMapWindow ? "1" : "0";
		contents += ";";
		contents += showCameraWindow ? "1" : "0";
		contents += ";";
		contents += showGameInputWindow ? "1" : "0";
		contents += ";";
		contents += showGameplayDiagnosticsWindow ? "1" : "0";
		contents += ";";
		contents += showAnimationDiagnosticsWindow ? "1" : "0";
		contents += ";";
		contents += showGameGUIDiagnosticsWindow ? "1" : "0";
		contents += ";";
		contents += profilerEnabled ? "1" : "0";
		contents += ";";
		contents += showCameraCollisionDebug ? "1" : "0";
		contents += ";";
		contents += showPhysicsDiagnosticsWindow ? "1" : "0";
		contents += "\n";

		const DirectionalLight& sunLight = renderManager.Lights().SunLight();
		contents += "sunlight;";
		contents += std::to_string(sunLight.direction.x) + ";" + std::to_string(sunLight.direction.y) + ";" + std::to_string(sunLight.direction.z) + ";";
		contents += std::to_string(sunLight.color.x) + ";" + std::to_string(sunLight.color.y) + ";" + std::to_string(sunLight.color.z) + ";";
		contents += std::to_string(sunLight.intensity) + ";";
		contents += std::to_string(sunLight.ambient) + ";";
		contents += (renderManager.Lights().ShadowsEnabled() ? "1;" : "0;");
		contents += (sunLight.castsShadows ? "1\n" : "0\n");

		for (const PointLight& pointLight : renderManager.Lights().PointLights())
		{
			contents += "pointlight;";
			contents += std::to_string(pointLight.position.x) + ";" + std::to_string(pointLight.position.y) + ";" + std::to_string(pointLight.position.z) + ";";
			contents += std::to_string(pointLight.color.x) + ";" + std::to_string(pointLight.color.y) + ";" + std::to_string(pointLight.color.z) + ";";
			contents += std::to_string(pointLight.intensity) + ";";
			contents += std::to_string(pointLight.ambient) + ";";
			contents += std::to_string(pointLight.radius) + ";";
			contents += std::to_string(pointLight.radiusFade) + ";";
			contents += std::to_string(pointLight.constant) + ";";
			contents += std::to_string(pointLight.linear) + ";";
			contents += std::to_string(pointLight.quadratic) + ";";
			contents += (pointLight.castsShadows ? "1\n" : "0\n");
		}
	}
}



