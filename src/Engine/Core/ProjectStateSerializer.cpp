#include "Engine/Core/ProjectStateSerializer.h"
#include "Engine/Core/ProjectStateFormat.h"

#include "Engine/Core/EntityStateMachine.h"
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

#include <glm/glm.hpp>
#include <istream>
#include <sstream>

namespace ProjectStateSerializer {
	using ProjectStateData::PendingComponent;
	using ProjectStateData::PendingController;
	using ProjectStateData::PendingLevel;
	using ProjectStateData::RenderStateData;

	namespace {
		bool DeserializeInputBinding(const std::vector<std::string>& fields, std::size_t& index, ProjectStateData::PendingInputAction::InputBindingData& binding)
		{
			if (index + 6 >= fields.size())
			{
				return false;
			}
			binding.type = std::stoi(fields[index++]);
			binding.code = std::stoi(fields[index++]);
			binding.joystick = std::stoi(fields[index++]);
			binding.scale = std::stof(fields[index++]);
			binding.vector.x = std::stof(fields[index++]);
			binding.vector.y = std::stof(fields[index++]);
			binding.stick = std::stoi(fields[index++]);
			return true;
		}

		bool IsIntegerField(const std::vector<std::string>& fields, std::size_t index)
		{
			if (index >= fields.size())
			{
				return false;
			}
			const std::string& value = fields[index];
			if (value.empty())
			{
				return false;
			}
			std::size_t start = (value[0] == '-' || value[0] == '+') ? 1 : 0;
			if (start >= value.size())
			{
				return false;
			}
			for (std::size_t i = start; i < value.size(); ++i)
			{
				if (!std::isdigit(static_cast<unsigned char>(value[i])))
				{
					return false;
				}
			}
			return true;
		}

		struct EntityStateParseResult
		{
			std::vector<PendingComponent::EntityStateData> states;
			std::vector<PendingComponent::EntityStateTransitionData> transitions;
			std::size_t nextIndex = 0;
			bool valid = false;
		};

		EntityStateParseResult ParseEntityStateComponent(const std::vector<std::string>& fields, std::size_t startIndex, int projectVersion, bool legacyStateInterruptField)
		{
			EntityStateParseResult result;
			std::size_t index = startIndex;
			try
			{
				int stateCount = std::stoi(fields.at(index++));
				if (stateCount < 0)
				{
					return result;
				}
				for (int i = 0; i < stateCount; ++i)
				{
					PendingComponent::EntityStateData state;
					state.name = ProjectStateFormat::UnescapeField(fields.at(index++));
					state.animationName = ProjectStateFormat::UnescapeField(fields.at(index++));
					if (legacyStateInterruptField)
					{
						(void)fields.at(index++);
					}
					if (projectVersion >= 20)
					{
						state.blocksMovement = fields.at(index++) == "1" || fields.at(index - 1) == "true" || fields.at(index - 1) == "True";
						state.blocksInput = fields.at(index++) == "1" || fields.at(index - 1) == "true" || fields.at(index - 1) == "True";
					}
					result.states.push_back(std::move(state));
				}

				int transitionCount = std::stoi(fields.at(index++));
				if (transitionCount < 0)
				{
					return result;
				}
				for (int i = 0; i < transitionCount; ++i)
				{
					PendingComponent::EntityStateTransitionData transition;
					transition.from = ProjectStateFormat::UnescapeField(fields.at(index++));
					transition.to = ProjectStateFormat::UnescapeField(fields.at(index++));
					transition.blendSeconds = std::stof(fields.at(index++));
					if (projectVersion >= 19)
					{
						const std::string interruptField = fields.at(index++);
						transition.interrupt = interruptField == "1" || interruptField == "true" || interruptField == "True";
					}
					if (projectVersion >= 17)
					{
						int conditionCount = std::stoi(fields.at(index++));
						for (int conditionIndex = 0; conditionIndex < conditionCount; ++conditionIndex)
						{
							PendingComponent::EntityStateConditionData condition;
							condition.left.type = std::stoi(fields.at(index++));
							condition.left.constantValue = std::stof(fields.at(index++));
							condition.left.componentName = ProjectStateFormat::UnescapeField(fields.at(index++));
							condition.left.memberName = ProjectStateFormat::UnescapeField(fields.at(index++));
							condition.comparator = std::stoi(fields.at(index++));
							condition.right.type = std::stoi(fields.at(index++));
							condition.right.constantValue = std::stof(fields.at(index++));
							condition.right.componentName = ProjectStateFormat::UnescapeField(fields.at(index++));
							condition.right.memberName = ProjectStateFormat::UnescapeField(fields.at(index++));
							transition.conditions.push_back(std::move(condition));
						}
					}
					else if (projectVersion >= 11)
					{
						transition.left.type = std::stoi(fields.at(index++));
						transition.left.constantValue = std::stof(fields.at(index++));
						transition.left.componentName = ProjectStateFormat::UnescapeField(fields.at(index++));
						transition.left.memberName = ProjectStateFormat::UnescapeField(fields.at(index++));
						transition.comparator = std::stoi(fields.at(index++));
						transition.right.type = std::stoi(fields.at(index++));
						transition.right.constantValue = std::stof(fields.at(index++));
						transition.right.componentName = ProjectStateFormat::UnescapeField(fields.at(index++));
						transition.right.memberName = ProjectStateFormat::UnescapeField(fields.at(index++));
					}
					result.transitions.push_back(std::move(transition));
				}
				result.nextIndex = index;
				result.valid = true;
			}
			catch (...)
			{
			}
			return result;
		}

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
			return type == "controller" || type == "playercontroller" || type == "enemy" || type == "entitystate";
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
						contents += ";" + std::to_string(playerController->TurnSpeed()) + "\n";
					}
					else if (const Controller* controller = dynamic_cast<const Controller*>(component))
					{
						AppendComponentLine(contents, projectPath, object, "controller");
						contents += ";" + std::to_string(controller->MoveSpeed()) + "\n";
					}
				else if (dynamic_cast<const Enemy*>(component))
				{
					AppendComponentLine(contents, projectPath, object, "enemy");
					contents += "\n";
				}
				else if (const EntityStateMachine* animator = dynamic_cast<const EntityStateMachine*>(component))
				{
					AppendComponentLine(contents, projectPath, object, "entitystate");
					contents += ";" + ProjectStateFormat::EscapeField(animator->InitialState());
					contents += ";" + std::to_string(animator->States().size());
					for (const auto& state : animator->States())
					{
						contents += ";" + ProjectStateFormat::EscapeField(state.name);
						contents += ";" + ProjectStateFormat::EscapeField(state.animationName);
						contents += ";" + std::to_string(state.blocksMovement ? 1 : 0);
						contents += ";" + std::to_string(state.blocksInput ? 1 : 0);
					}

					contents += ";" + std::to_string(animator->Transitions().size());
					for (const auto& transition : animator->Transitions())
					{
						const auto appendOperand = [&contents](const EntityStateMachine::Operand& operand)
						{
							contents += ";" + std::to_string(static_cast<int>(operand.type));
							contents += ";" + std::to_string(operand.constantValue);
							contents += ";" + ProjectStateFormat::EscapeField(operand.componentName);
							contents += ";" + ProjectStateFormat::EscapeField(operand.memberName);
						};

						contents += ";" + ProjectStateFormat::EscapeField(transition.from);
						contents += ";" + ProjectStateFormat::EscapeField(transition.to);
						contents += ";" + std::to_string(transition.blendSeconds);
						contents += ";" + std::to_string(transition.interrupt ? 1 : 0);
						const auto& conditions = transition.conditions.empty()
							? std::vector<EntityStateMachine::Condition>{ transition.condition }
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
		std::vector<ProjectStateData::PendingInputAction>& pendingInputActions,
	std::vector<std::string>& pendingGameGUIAssets,
	std::string& pendingActiveGameGUIAsset,
	std::string& pendingGameGUINavigationMode,
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

			try
			{
				const std::vector<std::string> fields = ProjectStateFormat::SplitFields(line);
				if ((fields.size() >= 7 && fields[0] == "gamecamera") || (fields.size() == 3 && fields[0] == "editorview") || (fields.size() >= 3 && fields[0] == "debugwindows") || ((fields.size() >= 8 && fields.size() <= 11) && fields[0] == "sunlight") || ((fields.size() >= 12 && fields.size() <= 15) && fields[0] == "pointlight") || (fields.size() == 2 && fields[0] == "imguilayout"))
				{
					if (fields.size() >= 7 && fields[0] == "gamecamera")
					{
						renderState.gameCameraPosition = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
						renderState.gameCameraFacing = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
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

		if (fields.size() >= 3 && fields[0] == "inputaction")
		{
			try
			{
				ProjectStateData::PendingInputAction action;
				action.name = ProjectStateFormat::UnescapeField(fields[1]);
				const std::size_t bindingCount = static_cast<std::size_t>(std::stoul(fields[2]));
				std::size_t index = 3;
				for (std::size_t i = 0; i < bindingCount; ++i)
				{
					ProjectStateData::PendingInputAction::InputBindingData binding;
					if (!DeserializeInputBinding(fields, index, binding))
					{
						break;
					}
					action.bindings.push_back(binding);
				}
				pendingInputActions.push_back(std::move(action));
			}
			catch (...)
			{
			}
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

		if (fields.size() == 2 && fields[0] == "gameguinavigationmode")
		{
			pendingGameGUINavigationMode = ProjectStateFormat::UnescapeField(fields[1]);
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
						}
						pendingControllers.push_back(std::move(controller));
						continue;
					}

					PendingComponent component;
					component.sourcePath = sourcePath;
					component.entityId = componentLayout.entityId;
					component.levelName = currentLevel->name;
					component.type = componentType;

					if (componentType == "enemy")
					{
						pendingComponents.push_back(std::move(component));
						continue;
					}

					if (componentType == "entitystate")
					{
						if (fields.size() < componentLayout.dataIndex + 2)
						{
							return false;
						}

						component.initialState = ProjectStateFormat::UnescapeField(fields[componentLayout.dataIndex]);
						const std::size_t dataStart = componentLayout.dataIndex + 1;
						const bool oldLayoutCandidate = fields.size() > dataStart + 1 && IsIntegerField(fields, dataStart + 1 + static_cast<std::size_t>(std::stoi(fields[dataStart])) * 5);
						const bool newLayoutCandidate = fields.size() > dataStart + 1 && IsIntegerField(fields, dataStart + 1 + static_cast<std::size_t>(std::stoi(fields[dataStart])) * 4);

						EntityStateParseResult parsed;
						if (oldLayoutCandidate)
						{
							parsed = ParseEntityStateComponent(fields, dataStart, projectVersion, true);
						}
						if (!parsed.valid && newLayoutCandidate)
						{
							parsed = ParseEntityStateComponent(fields, dataStart, projectVersion, false);
						}
						if (!parsed.valid)
						{
							return false;
						}

						component.entityStateStates = std::move(parsed.states);
						component.entityStateTransitions = std::move(parsed.transitions);
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
						const int colliderShape = std::stoi(fields[14]);
						object.physicsColliderShape = colliderShape == 1 ? 1 : colliderShape == 2 ? 2 : 0;
					}
					catch (...)
					{
						object.physicsColliderShape = 0;
					}
				}
				currentLevel->objects.push_back(std::move(object));
			}
			catch (const std::exception& ex)
			{
				Root::Current().Debugger().LogTagged(Debug::Severity::Warning, "ProjectLoad", std::string("Skipping malformed project line: ") + ex.what());
				continue;
			}
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





