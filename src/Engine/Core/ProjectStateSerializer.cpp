#include "Engine/Core/ProjectStateSerializer.h"
#include "Engine/Core/ProjectStateFormat.h"

#include "Engine/Core/AnimatorComponent.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/EngineGUI.h"
#include "Engine/Core/LevelManager.h"
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
			contents += componentType;
		}
	}

	std::string EscapeField(const std::string& value) { return ProjectStateFormat::EscapeField(value); }
	std::string UnescapeField(const std::string& value) { return ProjectStateFormat::UnescapeField(value); }
	std::vector<std::string> SplitFields(const std::string& line) { return ProjectStateFormat::SplitFields(line); }
	std::string HexEncode(const char* data, std::size_t size) { return ProjectStateFormat::HexEncode(data, size); }
	std::string HexDecode(const std::string& text) { return ProjectStateFormat::HexDecode(text); }
	std::filesystem::path MakePortableSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath) { return ProjectStateFormat::MakePortableSourcePath(projectPath, sourcePath); }
	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath) { return ProjectStateFormat::ResolveSourcePath(projectPath, sourcePath); }
	void AppendLevelState(std::string& contents, const std::filesystem::path& projectPath, const LevelManager& levelManager) { ProjectStateFormat::AppendLevelState(contents, projectPath, levelManager); }

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
			if ((fields.size() == 7 && fields[0] == "gamecamera") || (fields.size() == 3 && fields[0] == "editorview") || (fields.size() == 3 && fields[0] == "debugwindows") || ((fields.size() == 8 || fields.size() == 9) && fields[0] == "sunlight") || ((fields.size() == 12 || fields.size() == 13 || fields.size() == 14) && fields[0] == "pointlight") || (fields.size() == 2 && fields[0] == "imguilayout"))
			{
				if (fields.size() == 7 && fields[0] == "gamecamera")
				{
					renderState.gameCameraPosition = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
					renderState.gameCameraFacing = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
				}
				else if (fields.size() == 3 && fields[0] == "editorview")
				{
					renderState.editorShowAxis = fields[1] == "1" || fields[1] == "true" || fields[1] == "True";
					renderState.editorShowGrid = fields[2] == "1" || fields[2] == "true" || fields[2] == "True";
				}
				else if (fields.size() == 3 && fields[0] == "debugwindows")
				{
					renderState.debugShowLogWindow = fields[1] == "1" || fields[1] == "true" || fields[1] == "True";
					renderState.debugShowStatsWindow = fields[2] == "1" || fields[2] == "true" || fields[2] == "True";
				}
				else if ((fields.size() == 8 || fields.size() == 9) && fields[0] == "sunlight")
				{
					renderState.sunLight.direction = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
					renderState.sunLight.color = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
					renderState.sunLight.intensity = std::stof(fields[7]);
					if (fields.size() == 9)
					{
						renderState.sunLight.ambient = std::stof(fields[8]);
					}
				}
				else if ((fields.size() == 12 || fields.size() == 13 || fields.size() == 14) && fields[0] == "pointlight")
				{
					RenderStateData::PointLightData pointLight;
					pointLight.position = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
					pointLight.color = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
					pointLight.intensity = std::stof(fields[7]);
					if (fields.size() == 14)
					{
						pointLight.ambient = std::stof(fields[8]);
						pointLight.radius = std::stof(fields[9]);
						pointLight.radiusFade = std::stof(fields[10]);
						pointLight.constant = std::stof(fields[11]);
						pointLight.linear = std::stof(fields[12]);
						pointLight.quadratic = std::stof(fields[13]);
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

			if (fields.size() == 4 && fields[0] == "component" && (fields[2] == "controller" || fields[2] == "playercontroller"))
			{
				if (!currentLevel)
				{
					return false;
				}
				pendingControllers.push_back(PendingController{ ProjectStateFormat::ResolveSourcePath(projectPath, fields[1]), std::stof(fields[3]), currentLevel->name, fields[2] == "playercontroller" || projectVersion <= 11 });
				continue;
			}

			if (fields.size() >= 3 && fields[0] == "component" && fields[2] == "playerhealth")
			{
				if (!currentLevel)
				{
					return false;
				}
				PendingComponent component;
				component.sourcePath = ProjectStateFormat::ResolveSourcePath(projectPath, fields[1]);
				component.levelName = currentLevel->name;
				component.type = "playerhealth";
				if (fields.size() >= 4) { component.value1 = std::stoi(fields[3]); component.hasValue1 = true; }
				if (fields.size() >= 5) { component.value2 = std::stoi(fields[4]); component.hasValue2 = true; }
				pendingComponents.push_back(std::move(component));
				continue;
			}

			if (fields.size() == 3 && fields[0] == "component" && fields[2] == "enemy")
			{
				if (!currentLevel)
				{
					return false;
				}
				PendingComponent component;
				component.sourcePath = ProjectStateFormat::ResolveSourcePath(projectPath, fields[1]);
				component.levelName = currentLevel->name;
				component.type = "enemy";
				pendingComponents.push_back(std::move(component));
				continue;
			}

			if (fields.size() >= 6 && fields[0] == "component" && fields[2] == "animator")
			{
				if (!currentLevel)
				{
					return false;
				}
				PendingComponent component;
				component.sourcePath = ProjectStateFormat::ResolveSourcePath(projectPath, fields[1]);
				component.levelName = currentLevel->name;
				component.type = "animator";
				component.initialState = ProjectStateFormat::UnescapeField(fields[3]);
				const int stateCount = std::stoi(fields[4]);
				std::size_t index = 5;
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
					if (projectVersion >= 11)
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

			if (fields.size() >= 2 && fields[0] == "level")
			{
				PendingLevel level;
				level.name = ProjectStateFormat::UnescapeField(fields[1]);
				if (level.name.empty())
				{
					level.name = "Level";
				}
				if (fields.size() >= 3)
				{
					level.active = fields[2] == "1" || fields[2] == "true" || fields[2] == "True";
				}
				pendingLevels.push_back(std::move(level));
				currentLevel = &pendingLevels.back();
				continue;
			}

			if (fields.size() != 11 || fields[0] != "object")
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
			currentLevel->objects.push_back(std::move(object));
		}

		return true;
	}

	void AppendRenderState(std::string& contents, const FrontEndManager& frontEndManager, const RenderManager& renderManager)
	{
		const glm::vec3 gameCameraPosition = renderManager.GetGameCamera().GetPosition();
		const glm::vec3 gameCameraFacing = renderManager.GetGameCamera().GetFacing();
		contents += "gamecamera;";
		contents += std::to_string(gameCameraPosition.x) + ";" + std::to_string(gameCameraPosition.y) + ";" + std::to_string(gameCameraPosition.z) + ";";
		contents += std::to_string(gameCameraFacing.x) + ";" + std::to_string(gameCameraFacing.y) + ";" + std::to_string(gameCameraFacing.z) + "\n";

		contents += "editorview;";
		contents += frontEndManager.EditorGUI().ShowAxis() ? "1" : "0";
		contents += ";";
		contents += frontEndManager.EditorGUI().ShowGrid() ? "1" : "0";
		contents += "\n";
		contents += "debugwindows;";
		contents += Root::Current().Debugger().ShowLogWindow() ? "1" : "0";
		contents += ";";
		contents += Root::Current().Debugger().ShowStatsWindow() ? "1" : "0";
		contents += "\n";

		const DirectionalLight& sunLight = renderManager.Lights().SunLight();
		contents += "sunlight;";
		contents += std::to_string(sunLight.direction.x) + ";" + std::to_string(sunLight.direction.y) + ";" + std::to_string(sunLight.direction.z) + ";";
		contents += std::to_string(sunLight.color.x) + ";" + std::to_string(sunLight.color.y) + ";" + std::to_string(sunLight.color.z) + ";";
		contents += std::to_string(sunLight.intensity) + ";";
		contents += std::to_string(sunLight.ambient) + "\n";

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
			contents += std::to_string(pointLight.quadratic) + "\n";
		}
	}
}
