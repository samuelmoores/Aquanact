#include "Engine/ProjectStateSerializer.h"

#include "Engine/AnimatorComponent.h"
#include "Engine/Controller.h"
#include "Engine/Debug.h"
#include "Engine/Entity.h"
#include "Engine/FrontEndManager.h"
#include "Engine/Globals.h"
#include "Engine/FileSystem.h"
#include "Engine/LevelManager.h"
#include "Engine/PlayerController.h"
#include "Engine/RenderManager.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <cstring>
#include <glm/glm.hpp>
#include <imgui.h>
#include <istream>
#include <sstream>

namespace ProjectStateSerializer {
	namespace {
		void AppendComponentLine(std::string& contents, const std::filesystem::path& projectPath, const Entity* object, const char* componentType)
		{
			contents += "component;";
			contents += EscapeField(MakePortableSourcePath(projectPath, object->SourcePath()).string());
			contents += ";";
			contents += componentType;
		}
	}

	std::string EscapeField(const std::string& value)
	{
		std::string escaped;
		escaped.reserve(value.size());
		for (char ch : value)
		{
			if (ch == '\\' || ch == ';')
			{
				escaped.push_back('\\');
			}
			escaped.push_back(ch);
		}
		return escaped;
	}

	std::string UnescapeField(const std::string& value)
	{
		std::string unescaped;
		unescaped.reserve(value.size());
		bool escapeNext = false;
		for (char ch : value)
		{
			if (escapeNext)
			{
				unescaped.push_back(ch);
				escapeNext = false;
			}
			else if (ch == '\\')
			{
				escapeNext = true;
			}
			else
			{
				unescaped.push_back(ch);
			}
		}
		return unescaped;
	}

	std::vector<std::string> SplitFields(const std::string& line)
	{
		std::vector<std::string> fields;
		std::string current;
		bool escapeNext = false;
		for (char ch : line)
		{
			if (escapeNext)
			{
				current.push_back(ch);
				escapeNext = false;
			}
			else if (ch == '\\')
			{
				escapeNext = true;
			}
			else if (ch == ';')
			{
				fields.push_back(current);
				current.clear();
			}
			else
			{
				current.push_back(ch);
			}
		}
		fields.push_back(current);
		return fields;
	}

	std::string HexEncode(const char* data, std::size_t size)
	{
		static constexpr char digits[] = "0123456789ABCDEF";
		std::string encoded;
		encoded.reserve(size * 2);
		for (std::size_t i = 0; i < size; ++i)
		{
			const unsigned char byte = static_cast<unsigned char>(data[i]);
			encoded.push_back(digits[(byte >> 4) & 0xF]);
			encoded.push_back(digits[byte & 0xF]);
		}
		return encoded;
	}

	std::string HexDecode(const std::string& text)
	{
		auto hexValue = [](char ch) -> int
		{
			if (ch >= '0' && ch <= '9') return ch - '0';
			if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
			if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
			return -1;
		};

		std::string decoded;
		if (text.size() % 2 != 0)
		{
			return decoded;
		}
		decoded.reserve(text.size() / 2);
		for (std::size_t i = 0; i < text.size(); i += 2)
		{
			const int hi = hexValue(text[i]);
			const int lo = hexValue(text[i + 1]);
			if (hi < 0 || lo < 0)
			{
				return {};
			}
			decoded.push_back(static_cast<char>((hi << 4) | lo));
		}
		return decoded;
	}

	static bool IsRelativeToParent(const std::filesystem::path& relativePath)
	{
		auto it = relativePath.begin();
		if (it == relativePath.end())
		{
			return false;
		}

		return *it == std::filesystem::path("..");
	}

	static std::filesystem::path AssetsRoot()
	{
		return std::filesystem::path("C:/dev/Aquanact/assets");
	}

	static std::filesystem::path ModelsRoot()
	{
		return AssetsRoot() / "models";
	}

	static std::filesystem::path TexturesRoot()
	{
		return AssetsRoot() / "textures";
	}

	static std::filesystem::path ProjectsRoot()
	{
		return AssetsRoot() / "projects";
	}

	std::filesystem::path MakePortableSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath)
	{
		if (!gFileSystem.Exists(sourcePath))
		{
			return sourcePath;
		}

		const std::filesystem::path projectDir = projectPath.parent_path();
		std::error_code ec;
		const std::filesystem::path projectRelative = gFileSystem.Relative(sourcePath, projectDir, ec);
		if (!ec && !projectRelative.empty() && !IsRelativeToParent(projectRelative))
		{
			return projectRelative;
		}

		const std::filesystem::path assetsRoot = AssetsRoot();
		const std::filesystem::path assetsRelative = gFileSystem.Relative(sourcePath, assetsRoot, ec);
		if (!ec && !assetsRelative.empty() && !IsRelativeToParent(assetsRelative))
		{
			return assetsRelative;
		}

		return sourcePath;
	}

	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath)
	{
		if (gFileSystem.Exists(sourcePath))
		{
			return sourcePath;
		}

		const std::filesystem::path projectDir = projectPath.parent_path();
		const std::filesystem::path projectRelative = projectDir / sourcePath;
		if (gFileSystem.Exists(projectRelative))
		{
			return projectRelative;
		}

		const std::filesystem::path assetsRelative = AssetsRoot() / sourcePath;
		if (gFileSystem.Exists(assetsRelative))
		{
			return assetsRelative;
		}

		const std::filesystem::path searchRoots[] = { ModelsRoot(), TexturesRoot(), ProjectsRoot() };
		for (const auto& searchRoot : searchRoots)
		{
			if (!gFileSystem.Exists(searchRoot) || !gFileSystem.IsDirectory(searchRoot))
			{
				continue;
			}

			for (const auto& entry : gFileSystem.ReadDirectoryRecursive(searchRoot))
			{
				if (!entry.is_regular_file())
				{
					continue;
				}

				if (entry.path().filename() == sourcePath.filename())
				{
					return entry.path();
				}
			}
		}

		return projectRelative;
	}

	void AppendLevelState(std::string& contents, const std::filesystem::path& projectPath, const LevelManager& levelManager)
	{
		const auto& levels = levelManager.Levels();
		if (levels.empty())
		{
			contents += "level;Default;1\n";
			return;
		}

		for (const auto& level : levels)
		{
			if (!level)
			{
				continue;
			}

			contents += "level;";
			contents += EscapeField(level->Name());
			contents += ";";
			contents += (levelManager.ActiveLevel() == level.get()) ? "1" : "0";
			contents += "\n";

			for (const auto& object : level->Objects())
			{
				if (!object)
				{
					continue;
				}

				const glm::vec3 position = object->Position();
				const glm::vec3 rotation = object->Rotation();
				const glm::vec3 scale = object->Scale();

				const std::filesystem::path portableSourcePath = MakePortableSourcePath(projectPath, object->SourcePath());
				contents += "object;";
				contents += EscapeField(portableSourcePath.string());
				contents += ";";
				contents += std::to_string(position.x) + ";" + std::to_string(position.y) + ";" + std::to_string(position.z) + ";";
				contents += std::to_string(rotation.x) + ";" + std::to_string(rotation.y) + ";" + std::to_string(rotation.z) + ";";
				contents += std::to_string(scale.x) + ";" + std::to_string(scale.y) + ";" + std::to_string(scale.z) + "\n";
			}

			for (const auto& object : level->Objects())
			{
				if (!object)
				{
					continue;
				}

				if (const PlayerController* playerController = object->GetComponent<PlayerController>())
				{
					AppendComponentLine(contents, projectPath, object.get(), "playercontroller");
					contents += ";";
					contents += std::to_string(playerController->MoveSpeed());
					contents += "\n";
				}
				else if (Controller* controller = object->GetController())
				{
					AppendComponentLine(contents, projectPath, object.get(), "controller");
					contents += ";";
					contents += std::to_string(controller->MoveSpeed());
					contents += "\n";
				}

				if (const PlayerHealth* playerHealth = object->GetComponent<PlayerHealth>())
				{
					AppendComponentLine(contents, projectPath, object.get(), "playerhealth");
					contents += ";";
					contents += std::to_string(playerHealth->Health()) + ";" + std::to_string(playerHealth->MaxHealth());
					contents += "\n";
				}

				if (object->GetComponent<Enemy>())
				{
					AppendComponentLine(contents, projectPath, object.get(), "enemy");
					contents += "\n";
				}

				if (const AnimatorComponent* animator = object->GetComponent<AnimatorComponent>())
				{
					AppendComponentLine(contents, projectPath, object.get(), "animator");
					contents += ";";
					contents += EscapeField(animator->InitialState());
					contents += ";";
					contents += std::to_string(animator->States().size());
					for (const auto& state : animator->States())
					{
						contents += ";";
						contents += EscapeField(state.name);
						contents += ";";
						contents += std::to_string(state.clipIndex);
					}
					contents += ";";
					contents += std::to_string(animator->Transitions().size());
					for (const auto& transition : animator->Transitions())
					{
						contents += ";";
						contents += EscapeField(transition.from);
						contents += ";";
						contents += EscapeField(transition.to);
						contents += ";";
						contents += std::to_string(transition.blendSeconds);
						contents += ";";
						contents += std::to_string(static_cast<int>(transition.condition.left.type));
						contents += ";";
						contents += std::to_string(transition.condition.left.constantValue);
						contents += ";";
						contents += EscapeField(transition.condition.left.componentName);
						contents += ";";
						contents += EscapeField(transition.condition.left.memberName);
						contents += ";";
						contents += std::to_string(static_cast<int>(transition.condition.comparator));
						contents += ";";
						contents += std::to_string(static_cast<int>(transition.condition.right.type));
						contents += ";";
						contents += std::to_string(transition.condition.right.constantValue);
						contents += ";";
						contents += EscapeField(transition.condition.right.componentName);
						contents += ";";
						contents += EscapeField(transition.condition.right.memberName);
					}
					contents += "\n";
				}
			}
		}
	}

	bool LoadLevelState(const std::filesystem::path& projectPath, std::istream& file, int projectVersion, LevelManager& levelManager, FrontEndManager& frontEndManager, RenderManager& renderManager, std::vector<std::string>& pendingGameGUIAssets, std::string& pendingActiveGameGUIAsset, std::string& pendingImguiLayout)
	{
		std::vector<PendingController> pendingControllers;
		std::vector<PendingComponent> pendingComponents;
		std::vector<PendingLevel> pendingLevels;
		PendingLevel* currentLevel = nullptr;
		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty())
			{
				continue;
			}
			const std::vector<std::string> fields = SplitFields(line);
			if ((fields.size() == 7 && fields[0] == "gamecamera") || (fields.size() == 3 && fields[0] == "editorview") || ((fields.size() == 8 || fields.size() == 9) && fields[0] == "sunlight") || ((fields.size() == 12 || fields.size() == 13 || fields.size() == 14) && fields[0] == "pointlight") || (fields.size() == 2 && fields[0] == "imguilayout"))
			{
				ApplyRenderState(fields, frontEndManager, renderManager, pendingImguiLayout);
				continue;
			}
			if (fields.size() == 2 && fields[0] == "startuplevel")
			{
				levelManager.ApplyProjectState(UnescapeField(fields[1]));
				continue;
			}
			if (fields.size() == 2 && fields[0] == "gameguiasset")
			{
				pendingGameGUIAssets.push_back(UnescapeField(fields[1]));
				continue;
			}
			if (fields.size() == 2 && fields[0] == "gameguiactive")
			{
				pendingActiveGameGUIAsset = UnescapeField(fields[1]);
				continue;
			}
			if (fields.size() == 4 && fields[0] == "component" && (fields[2] == "controller" || fields[2] == "playercontroller"))
			{
				if (!currentLevel) return false;
				pendingControllers.push_back(PendingController{ ResolveSourcePath(projectPath, fields[1]), std::stof(fields[3]), currentLevel->name, fields[2] == "playercontroller" || projectVersion <= 11 });
				continue;
			}
			if (fields.size() >= 3 && fields[0] == "component" && fields[2] == "playerhealth")
			{
				if (!currentLevel) return false;
				PendingComponent component;
				component.sourcePath = ResolveSourcePath(projectPath, fields[1]);
				component.levelName = currentLevel->name;
				component.type = "playerhealth";
				if (fields.size() >= 4) { component.value1 = std::stoi(fields[3]); component.hasValue1 = true; }
				if (fields.size() >= 5) { component.value2 = std::stoi(fields[4]); component.hasValue2 = true; }
				pendingComponents.push_back(std::move(component));
				continue;
			}
			if (fields.size() == 3 && fields[0] == "component" && fields[2] == "enemy")
			{
				if (!currentLevel) return false;
				PendingComponent component;
				component.sourcePath = ResolveSourcePath(projectPath, fields[1]);
				component.levelName = currentLevel->name;
				component.type = "enemy";
				pendingComponents.push_back(std::move(component));
				continue;
			}
			if (fields.size() >= 6 && fields[0] == "component" && fields[2] == "animator")
			{
				if (!currentLevel) return false;
				PendingComponent component;
				component.sourcePath = ResolveSourcePath(projectPath, fields[1]);
				component.levelName = currentLevel->name;
				component.type = "animator";
				component.initialState = UnescapeField(fields[3]);
				const int stateCount = std::stoi(fields[4]);
				std::size_t index = 5;
				for (int i = 0; i < stateCount; ++i) { PendingComponent::AnimatorStateData state; state.name = UnescapeField(fields[index++]); state.clipIndex = std::stoi(fields[index++]); component.animatorStates.push_back(std::move(state)); }
				const int transitionCount = std::stoi(fields[index++]);
				for (int i = 0; i < transitionCount; ++i) { PendingComponent::AnimatorTransitionData transition; transition.from = UnescapeField(fields[index++]); transition.to = UnescapeField(fields[index++]); transition.blendSeconds = std::stof(fields[index++]); if (projectVersion >= 11) { transition.left.type = std::stoi(fields[index++]); transition.left.constantValue = std::stof(fields[index++]); transition.left.componentName = UnescapeField(fields[index++]); transition.left.memberName = UnescapeField(fields[index++]); transition.comparator = std::stoi(fields[index++]); transition.right.type = std::stoi(fields[index++]); transition.right.constantValue = std::stof(fields[index++]); transition.right.componentName = UnescapeField(fields[index++]); transition.right.memberName = UnescapeField(fields[index++]); } component.animatorTransitions.push_back(std::move(transition)); }
				pendingComponents.push_back(std::move(component));
				continue;
			}
			if (fields.size() >= 2 && fields[0] == "level")
			{
				PendingLevel level;
				level.name = UnescapeField(fields[1]);
				if (level.name.empty()) level.name = "Level";
				if (fields.size() >= 3) level.active = fields[2] == "1" || fields[2] == "true" || fields[2] == "True";
				pendingLevels.push_back(std::move(level));
				currentLevel = &pendingLevels.back();
				continue;
			}
			if (fields.size() != 11 || fields[0] != "object") continue;
			if (!currentLevel) return false;
			const std::filesystem::path sourcePath = ResolveSourcePath(projectPath, fields[1]);
			auto object = std::make_unique<Entity>(sourcePath.string().c_str());
			object->Translate(glm::vec3(std::stof(fields[2]), std::stof(fields[3]), std::stof(fields[4])));
			object->SetRotation(glm::vec3(std::stof(fields[5]), std::stof(fields[6]), std::stof(fields[7])));
			object->SetScale(glm::vec3(std::stof(fields[8]), std::stof(fields[9]), std::stof(fields[10])));
			object->SetIgnoreCameraCollision(true);
			object->SetDefaultPosition(object->Position());
			object->SetDefaultRotation(object->Rotation());
			currentLevel->objects.push_back(std::move(object));
		}

		levelManager.Clear();
		if (pendingLevels.empty()) return false;
		Level* activeLevel = nullptr;
		for (auto& pendingLevel : pendingLevels)
		{
			Level* level = levelManager.CreateLevel(pendingLevel.name);
			if (!level) continue;
			if (pendingLevel.active) activeLevel = level;
			for (auto& object : pendingLevel.objects) level->AddObject(std::move(object));
		}
		for (const auto& pendingComponent : pendingComponents)
		{
			Level* level = levelManager.FindLevel(pendingComponent.levelName);
			if (!level)
			{
				continue;
			}

			for (const auto& object : level->Objects())
			{
				if (!object || object->SourcePath() != pendingComponent.sourcePath.string())
				{
					continue;
				}

				if (pendingComponent.type == "playercontroller")
				{
					if (!object->GetComponent<PlayerController>())
					{
						object->AddComponent<PlayerController>();
					}
					if (PlayerController* playerController = object->GetComponent<PlayerController>())
					{
						playerController->SetMoveSpeed(pendingComponent.value1);
					}
				}
				else if (pendingComponent.type == "controller")
				{
					if (!object->GetController())
					{
						object->AddComponent<Controller>();
					}
					if (Controller* controller = object->GetController())
					{
						controller->SetMoveSpeed(pendingComponent.value1);
					}
				}
				else if (pendingComponent.type == "playerhealth")
				{
					if (!object->GetComponent<PlayerHealth>())
					{
						object->AddComponent<PlayerHealth>();
					}
					if (PlayerHealth* playerHealth = object->GetComponent<PlayerHealth>())
					{
						if (pendingComponent.hasValue1)
						{
							playerHealth->SetHealth(pendingComponent.value1);
						}
						if (pendingComponent.hasValue2)
						{
							playerHealth->SetMaxHealth(pendingComponent.value2);
						}
					}
				}
				else if (pendingComponent.type == "enemy")
				{
					if (!object->GetComponent<Enemy>())
					{
						object->AddComponent<Enemy>();
					}
				}
				else if (pendingComponent.type == "animator")
				{
					if (!object->GetComponent<AnimatorComponent>())
					{
						object->AddComponent<AnimatorComponent>(object->GetMesh());
					}
					if (AnimatorComponent* animator = object->GetComponent<AnimatorComponent>())
					{
						animator->SetInitialState(pendingComponent.initialState);
						for (const auto& state : pendingComponent.animatorStates)
						{
							animator->AddState(state.name, state.clipIndex);
						}
						for (const auto& transition : pendingComponent.animatorTransitions)
						{
							AnimatorComponent::Condition condition;
							condition.left.type = static_cast<AnimatorComponent::OperandType>(transition.left.type);
							condition.left.constantValue = transition.left.constantValue;
							condition.left.componentName = transition.left.componentName;
							condition.left.memberName = transition.left.memberName;
							condition.comparator = static_cast<AnimatorComponent::Comparator>(transition.comparator);
							condition.right.type = static_cast<AnimatorComponent::OperandType>(transition.right.type);
							condition.right.constantValue = transition.right.constantValue;
							condition.right.componentName = transition.right.componentName;
							condition.right.memberName = transition.right.memberName;
							animator->AddTransition(transition.from, transition.to, transition.blendSeconds, condition);
						}
					}
				}
			}
		}
		if (!activeLevel) activeLevel = levelManager.Levels().front().get();
		levelManager.SetActiveLevel(activeLevel->Name());
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

	void ApplyRenderState(const std::vector<std::string>& fields, FrontEndManager& frontEndManager, RenderManager& renderManager, std::string& pendingImguiLayout)
	{
		if (fields.size() == 7 && fields[0] == "gamecamera")
		{
			renderManager.GetGameCamera().SetPose(
				glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3])),
				glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6])));
			return;
		}

		if (fields.size() == 3 && fields[0] == "editorview")
		{
			frontEndManager.EditorGUI().SetShowAxis(fields[1] == "1" || fields[1] == "true" || fields[1] == "True");
			frontEndManager.EditorGUI().SetShowGrid(fields[2] == "1" || fields[2] == "true" || fields[2] == "True");
			return;
		}

		if ((fields.size() == 8 || fields.size() == 9) && fields[0] == "sunlight")
		{
			DirectionalLight& sunLight = renderManager.Lights().SunLight();
			sunLight.direction = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
			sunLight.color = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
			sunLight.intensity = std::stof(fields[7]);
			if (fields.size() == 9)
			{
				sunLight.ambient = std::stof(fields[8]);
			}
			return;
		}

		if ((fields.size() == 12 || fields.size() == 13 || fields.size() == 14) && fields[0] == "pointlight")
		{
			PointLight& pointLight = renderManager.Lights().AddPointLight();
			pointLight.position = glm::vec3(std::stof(fields[1]), std::stof(fields[2]), std::stof(fields[3]));
			pointLight.color = glm::vec3(std::stof(fields[4]), std::stof(fields[5]), std::stof(fields[6]));
			pointLight.intensity = std::stof(fields[7]);
			if (fields.size() == 14)
			{
				pointLight.ambient = std::stof(fields[8]);
				pointLight.SetRadius(std::stof(fields[9]));
				pointLight.radiusFade = std::stof(fields[10]);
			}
			else
			{
				pointLight.SetRadius(std::stof(fields[8]));
				if (fields.size() == 13)
				{
					pointLight.radiusFade = std::stof(fields[9]);
				}
			}
			return;
		}

		if (fields.size() == 2 && fields[0] == "imguilayout")
		{
			pendingImguiLayout = HexDecode(fields[1]);
			return;
		}
	}
}
