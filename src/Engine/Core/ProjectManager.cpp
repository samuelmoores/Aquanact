#include "Engine/Core/ProjectManager.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/FileManager.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectStateSerializer.h"
#include "Engine/Core/SpawnManager.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/InputManager.h"
#include <algorithm>
#include <fstream>
#include <imgui.h>
#include <sstream>
#include <vector>

namespace {
	void SaveBuildProjectRootPreference(const std::filesystem::path& projectPath)
	{
#if defined(AQUANACT_PROJECT_ROOT_PREFERENCE_FILE) && !defined(AQUANACT_GAME)
		if (!Root::HasCurrent() || !Root::Current().State().IsEditorMode() || projectPath.empty())
		{
			return;
		}

		std::error_code error;
		const std::filesystem::path projectRoot =
			std::filesystem::absolute(projectPath, error).parent_path().lexically_normal();
		if (error)
		{
			return;
		}

		std::ofstream preferenceFile(AQUANACT_PROJECT_ROOT_PREFERENCE_FILE, std::ios::trunc);
		if (preferenceFile.is_open())
		{
			preferenceFile << projectRoot.string() << '\n';
			if (!preferenceFile)
			{
				Root::Current().Debugger().LogMessage(
					"Could not save the selected project root for the next CMake build.");
			}
		}
		else
		{
			Root::Current().Debugger().LogMessage(
				"Could not open the selected project root preference for the next CMake build.");
		}
#else
		(void)projectPath;
#endif
	}

	void AppendCurrentCameraState(std::string& contents)
	{
		const PathedCamera& gameCamera = Root::Current().Render().GetPathedCamera();
		const glm::vec3 gameCameraPosition = gameCamera.GetPosition();
		const glm::vec3 gameCameraFacing = gameCamera.GetFacing();
		contents += "gamecamera;";
		contents += std::to_string(gameCameraPosition.x) + ";" + std::to_string(gameCameraPosition.y) + ";" + std::to_string(gameCameraPosition.z) + ";";
		contents += std::to_string(gameCameraFacing.x) + ";" + std::to_string(gameCameraFacing.y) + ";" + std::to_string(gameCameraFacing.z) + ";";
		contents += "0;0;0;;0";
		contents += "\n";
	}

	void AppendImguiLayoutState(std::string& contents)
	{
		size_t imguiIniSize = 0;
		if (const char* imguiIniData = ImGui::SaveIniSettingsToMemory(&imguiIniSize))
		{
			contents += "imguilayout;";
			contents += ProjectStateSerializer::HexEncode(imguiIniData, imguiIniSize);
			contents += "\n";
		}
	}

	void AppendProjectStateSnapshot(std::string& contents, const std::filesystem::path& path, const SceneManager& SceneManager)
	{
		ProjectStateSerializer::AppendLevelState(contents, path, SceneManager);
	Root::Current().Spawns().AppendProjectState(contents, path, SceneManager);
		for (const auto& [actionName, bindings] : Root::Current().InputActions().Bindings())
		{
			contents += "inputaction;";
			contents += ProjectStateSerializer::EscapeField(actionName);
			contents += ";";
			contents += std::to_string(bindings.size());
			for (const InputBinding& binding : bindings)
			{
				contents += ";";
				contents += std::to_string(static_cast<int>(binding.type)) + ";" +
					std::to_string(binding.code) + ";" +
					std::to_string(binding.joystick) + ";" +
					std::to_string(binding.scale) + ";" +
					std::to_string(binding.vector.x) + ";" +
					std::to_string(binding.vector.y) + ";" +
					std::to_string(static_cast<int>(binding.stick));
			}
			contents += "\n";
		}
		ProjectStateSerializer::AppendRenderState(contents, Root::Current().FrontEnd(), Root::Current().Render());
		SceneManager.AppendProjectState(contents);
		if (SceneManager.StartupLevelName().empty())
		{
			contents += "startuplevel;MainMenu\n";
		}
		Root::Current().FrontEnd().RuntimeGUI().AppendProjectState(contents);
		AppendImguiLayoutState(contents);
	}

	void MaterializePendingLevels(SceneManager& SceneManager, const std::vector<ProjectStateData::PendingLevel>& pendingLevels)
	{
		SceneManager.Clear();
		for (const auto& pendingLevel : pendingLevels)
		{
			Root::Current().Debugger().LogTagged("ProjectLoad", "Materializing scene: " + pendingLevel.name);
			Scene* Scene = pendingLevel.isCutscene
				? SceneManager.CreateCutscene(pendingLevel.name)
				: SceneManager.CreateLevel(pendingLevel.name);
			if (!Scene)
			{
				continue;
			}
			SceneManager.SetSceneKind(pendingLevel.name, pendingLevel.isCutscene ? SceneManager::SceneKind::Cutscene : SceneManager::SceneKind::Level);
			Scene->Cutscene() = pendingLevel.cutscene;
			LightingManager& lighting = Scene->Lights();
			if (pendingLevel.hasSunLight)
			{
				if (!pendingLevel.sunLight.name.empty())
					lighting.SunLight().name = pendingLevel.sunLight.name;
				lighting.SunLight().direction = pendingLevel.sunLight.direction;
				lighting.SunLight().color = pendingLevel.sunLight.color;
				lighting.SunLight().intensity = pendingLevel.sunLight.intensity;
				lighting.SunLight().ambient = pendingLevel.sunLight.ambient;
				lighting.SunLight().castsShadows = pendingLevel.sunLight.castsShadows;
				lighting.SetShadowsEnabled(pendingLevel.sunLight.shadowsEnabled);
			}
			lighting.PointLights().clear();
			for (const ProjectStateData::PointLightData& pointLightData : pendingLevel.pointLights)
			{
				PointLight& pointLight = lighting.AddPointLight();
				if (!pointLightData.name.empty())
					pointLight.name = pointLightData.name;
				pointLight.position = pointLightData.position;
				pointLight.color = pointLightData.color;
				pointLight.intensity = pointLightData.intensity;
				pointLight.ambient = pointLightData.ambient;
				pointLight.SetRadius(pointLightData.radius);
				pointLight.radiusFade = pointLightData.radiusFade;
				pointLight.constant = pointLightData.constant;
				pointLight.linear = pointLightData.linear;
				pointLight.quadratic = pointLightData.quadratic;
				pointLight.castsShadows = pointLightData.castsShadows;
			}
			lighting.SpotLights().clear();
			for (const ProjectStateData::SpotLightData& spotLightData : pendingLevel.spotLights)
			{
				SpotLight& spotLight = lighting.AddSpotLight();
				if (!spotLightData.name.empty())
					spotLight.name = spotLightData.name;
				spotLight.position = spotLightData.position;
				spotLight.direction = glm::length(spotLightData.direction) > 0.001f
					? glm::normalize(spotLightData.direction) : glm::vec3(0.0f, -1.0f, 0.0f);
				spotLight.color = spotLightData.color;
				spotLight.intensity = spotLightData.intensity;
				spotLight.ambient = spotLightData.ambient;
				spotLight.SetRadius(spotLightData.radius);
				spotLight.radiusFade = spotLightData.radiusFade;
				spotLight.SetAngles(spotLightData.innerAngle, spotLightData.outerAngle);
				spotLight.constant = spotLightData.constant;
				spotLight.linear = spotLightData.linear;
				spotLight.quadratic = spotLightData.quadratic;
				spotLight.castsShadows = spotLightData.castsShadows;
			}
			if (pendingLevel.isMainMenu)
			{
				SceneManager.SetSceneKind(pendingLevel.name, SceneManager::SceneKind::Cutscene);
			}

			bool loadedGameManager = false;
			for (const auto& pendingObject : pendingLevel.objects)
			{
				// Scenes own a generated, non-rendering GameManager entity. Reuse it
				// for persisted manager records so serialized component IDs still
				// resolve, and collapse duplicates from older project saves.
				if (pendingObject.name == "GameManager"
					|| pendingObject.name == Scene->GameManagerEntityName())
				{
					if (loadedGameManager)
						continue;
					loadedGameManager = true;
					if (Entity* gameManager = Scene->GameManagerEntity())
					{
						if (pendingObject.id != 0)
							gameManager->SetId(pendingObject.id);
						continue;
					}
				}

				Root::Current().Debugger().LogTagged("ProjectLoad", "Creating object from source: " + pendingObject.sourcePath.string() + " in scene: " + pendingLevel.name);
				// Project component records are authoritative. Imported models still receive
				// their default components through Entity's default constructor behavior.
				std::unique_ptr<Entity> newObject;
				if (pendingObject.sourcePath.empty())
				{
					newObject = std::make_unique<Entity>(
						pendingObject.name.empty() ? std::string("Entity") : pendingObject.name,
						false);
				}
				else
				{
					newObject = std::make_unique<Entity>(pendingObject.sourcePath.string().c_str(), false);
				}
				Entity* object = newObject.get();
				if (!pendingObject.name.empty() && !pendingObject.sourcePath.empty())
				{
					object->SetName(pendingObject.name);
				}
				if (pendingObject.id != 0)
				{
					object->SetId(pendingObject.id);
				}
				object->Translate(pendingObject.position);
				object->SetRotation(pendingObject.rotation);
				object->SetScale(pendingObject.scale);
				object->SetBlocksCollision(pendingObject.blocksCollision);
				object->SetIgnoreCameraCollision(pendingObject.ignoreCameraCollision);
				object->SetBlocksCameraView(pendingObject.blocksCameraView);
				object->SetShowPhysicsBoundingBox(pendingObject.showPhysicsBoundingBox);
				object->SetRigidBody(pendingObject.rigidBody);
				object->SetMass(pendingObject.mass);
				object->SetLinearDamping(pendingObject.linearDamping);
				object->SetPhysicsColliderShape(pendingObject.physicsColliderShape == 1
					? PhysicsColliderShape::Capsule
					: pendingObject.physicsColliderShape == 2 ? PhysicsColliderShape::Convex : PhysicsColliderShape::Box);
				object->SetUseBaseMeshCollider(pendingObject.useBaseMeshCollider);
				object->SetCapsuleRadiusOverride(pendingObject.capsuleRadiusOverride);
				if (pendingObject.isSubmeshComponentEntity)
					object->SetSubmeshComponentOwner(pendingObject.submeshComponentParentId,
						pendingObject.submeshComponentIndex);
				for (std::size_t savedIndex = 0; savedIndex < pendingObject.submeshColliderSettings.size(); ++savedIndex)
				{
					const auto& saved = pendingObject.submeshColliderSettings[savedIndex];
					std::size_t submeshIndex = savedIndex;
					if (!saved.name.empty() && object->GetMesh())
						for (int meshIndex = 0; meshIndex < object->GetMesh()->NumBuffers(); ++meshIndex)
							if (object->GetMesh()->SubMeshName(meshIndex) == saved.name) { submeshIndex = static_cast<std::size_t>(meshIndex); break; }
					object->SetSubmeshColliderSettings(submeshIndex,
						{ saved.enabled, static_cast<PhysicsColliderShape>(saved.shape), saved.useEntityDefaultShape });
				}
				if (Mesh* mesh = object->GetMesh())
				{
					for (std::size_t savedIndex = 0; savedIndex < pendingObject.submeshMaterialSettings.size(); ++savedIndex)
					{
						const auto& saved = pendingObject.submeshMaterialSettings[savedIndex];
						std::size_t submeshIndex = savedIndex;
						if (!saved.name.empty())
							for (int meshIndex = 0; meshIndex < mesh->NumBuffers(); ++meshIndex)
								if (mesh->SubMeshName(meshIndex) == saved.name) { submeshIndex = static_cast<std::size_t>(meshIndex); break; }
						if (submeshIndex >= static_cast<std::size_t>(mesh->NumBuffers())) continue;
						const int index = static_cast<int>(submeshIndex);
						mesh->SetEmissiveEnabled(index, saved.emissiveEnabled);
						mesh->SetEmissiveColor(index, saved.emissiveColor);
						mesh->SetEmissiveBlendFactor(index, saved.emissiveBlendFactor);
						mesh->SetCastsShadow(index, saved.castsShadow);
						mesh->SetColorTextureEnabled(index, saved.colorTextureEnabled);
						mesh->SetSpecularTextureEnabled(index, saved.specularTextureEnabled);
						mesh->SetNormalTextureEnabled(index, saved.normalTextureEnabled);
						mesh->SetRoughnessTextureEnabled(index, saved.roughnessTextureEnabled);
						mesh->SetHeightTextureEnabled(index, saved.heightTextureEnabled);
						if (!saved.baseColorTexturePath.empty())
							mesh->SetColorTextureFile(index, saved.baseColorTexturePath);
						if (!saved.roughnessTexturePath.empty())
							mesh->SetRoughnessTextureFile(index, saved.roughnessTexturePath);
						if (saved.roughnessValue >= 0.0f)
							mesh->SetRoughnessValue(index, saved.roughnessValue);
					}
				}
				object->SetDefaultPosition(object->Position());
				object->SetDefaultRotation(object->Rotation());
				if (newObject)
					Scene->AddObject(std::move(newObject));
			}
			for (const auto& pendingObject : pendingLevel.objects)
			{
				if (!pendingObject.isSubmeshComponentEntity)
					continue;
				Entity* child = nullptr;
				Entity* parent = nullptr;
				for (const auto& candidate : Scene->Objects())
				{
					if (!candidate) continue;
					if (candidate->Id() == pendingObject.id) child = candidate.get();
					if (candidate->Id() == pendingObject.submeshComponentParentId) parent = candidate.get();
				}
				Mesh* mesh = parent ? parent->GetMesh() : nullptr;
				if (!child)
					continue;
				if (!mesh || pendingObject.submeshComponentIndex < 0
					|| pendingObject.submeshComponentIndex >= mesh->NumBuffers())
				{
					child->SetSubmeshComponentOwner(0, -1);
					continue;
				}
				const int index = pendingObject.submeshComponentIndex;
				const glm::vec3 localCenter = (mesh->SubMeshMinBounds(index)
					+ mesh->SubMeshMaxBounds(index)) * 0.5f;
				child->AttachTo(*parent, localCenter);
			}
			for (const auto& pendingCollider : pendingLevel.levelColliders)
			{
				auto collider = std::make_unique<LevelCollider>(pendingCollider.name);
				collider->SetShape(static_cast<LevelColliderShape>(std::clamp(pendingCollider.shape, 0, 1)));
				collider->SetPosition(pendingCollider.position);
				collider->SetRotation(pendingCollider.rotation);
				collider->SetScale(pendingCollider.scale);
				collider->SetRadius(pendingCollider.radius);
				collider->SetHeight(pendingCollider.height);
				collider->SetLayer(pendingCollider.layer);
				collider->SetMask(pendingCollider.mask);
				collider->SetTrigger(pendingCollider.trigger);
				collider->SetDebugVisible(pendingCollider.debugVisible);
				Scene->AddLevelCollider(std::move(collider));
			}
		}

	}

	void ApplyStartupLevel(SceneManager& SceneManager, const std::string& startupLevelName, const std::vector<ProjectStateData::PendingLevel>& pendingLevels)
	{
		if (!pendingLevels.empty())
		{
			const Scene* activeLevel = nullptr;
			for (const auto& pendingLevel : pendingLevels)
			{
				if (pendingLevel.active)
				{
					activeLevel = SceneManager.FindLevel(pendingLevel.name);
					break;
				}
			}
			if (!activeLevel)
			{
				activeLevel = SceneManager.Levels().front().get();
			}
			SceneManager.SetActiveLevel(activeLevel->Name());
		}

		if (!startupLevelName.empty())
		{
			SceneManager.ApplyProjectState(startupLevelName);
		}
		else
		{
			SceneManager.SetStartupLevelName("MainMenu");
		}
	}

	void RestorePathedCameraTarget(SceneManager& SceneManager, const ProjectStateData::RenderStateData& renderState)
	{
		PathedCamera& gameCamera = Root::Current().Render().GetPathedCamera();
		Root::Current().Debugger().LogTagged(
			"ProjectLoad",
			"Restoring camera target id=" + std::to_string(renderState.gameCameraTargetId) + " hasTarget=" + std::string(renderState.gameCameraHasTarget ? "true" : "false"));
		if (!renderState.gameCameraHasTarget || renderState.gameCameraTargetId == 0)
		{
			return;
		}

		Scene* activeLevel = SceneManager.ActiveLevel();
		if (!activeLevel)
		{
			return;
		}

		for (const auto& object : activeLevel->Objects())
		{
			if (!object)
			{
				continue;
			}
			Root::Current().Debugger().LogTagged("ProjectLoad", "Checking camera target candidate id=" + std::to_string(object->Id()) + " name=" + object->Name());
			if (object->Id() != renderState.gameCameraTargetId)
			{
				continue;
			}

			Root::Current().Debugger().LogTagged("ProjectLoad", "Camera target restored to id=" + std::to_string(object->Id()));
			return;
		}
		Root::Current().Debugger().LogTagged(Debug::Severity::Warning, "ProjectLoad", "Camera target id not found in active scene: " + std::to_string(renderState.gameCameraTargetId));
	}

	void EnsureMainMenuLevel(SceneManager& SceneManager)
	{
		if (!SceneManager.FindLevel("MainMenu"))
		{
			SceneManager.CreateCutscene("MainMenu");
		}
		SceneManager.SetSceneKind("MainMenu", SceneManager::SceneKind::Cutscene);
	}

}

ProjectManager::ProjectManager(FileSystem& fileSystem)
	: m_fileSystem(&fileSystem)
{
}

const std::filesystem::path& ProjectManager::CurrentProjectPath() const
{
	return m_currentProjectPath;
}

std::filesystem::path ProjectManager::PendingClassConfigurationPath() const
{
	if (!m_currentProjectPath.empty())
		return m_currentProjectPath.parent_path() / "NewClassConfiguration";
	return std::filesystem::current_path() / "NewClassConfiguration";
}

std::filesystem::path ProjectManager::ProjectDirectory() const
{
	if (!m_currentProjectPath.empty())
	{
		return m_currentProjectPath.parent_path();
	}
	return ProjectsRoot();
}

std::filesystem::path ProjectManager::ProjectsRoot() const
{
#ifdef AQUANACT_SOURCE_ROOT
	return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "projects";
#else
	return m_fileSystem ? m_fileSystem->ExecutableDirectory() / "projects" : std::filesystem::current_path() / "projects";
#endif
}

std::filesystem::path ProjectManager::ProjectAssetsDirectory() const
{
	return ProjectDirectory() / "assets";
}

bool ProjectManager::SaveProject(const std::filesystem::path& path, const SceneManager& SceneManager)
{
	if (!m_fileSystem)
	{
		return false;
	}

		std::string contents = "AquanactProject\n";
		AppendProjectStateSnapshot(contents, path, SceneManager);

	const bool written = m_fileSystem->WriteTextFile(path, contents);
	if (written)
	{
		m_currentProjectPath = path;
		SaveBuildProjectRootPreference(m_currentProjectPath);
		Root::Current().Files().SetRootDirectory(ProjectAssetsDirectory() / "models");
	}
	return written;
}

bool ProjectManager::LoadProject(const std::filesystem::path& path, SceneManager& SceneManager)
{
	Root::Current().Debugger().BeginProjectLoadLog(path);
	if (!m_fileSystem)
	{
		Root::Current().Debugger().LogProjectLoadStatus("File system is unavailable");
		Root::Current().Debugger().EndProjectLoadLog(false);
		return false;
	}

	Root::Current().Debugger().LogProjectLoadStatus("Reading project file");
	std::string fileContents = m_fileSystem->ReadTextFile(path);
	if (fileContents.empty())
	{
		Root::Current().Debugger().LogProjectLoadStatus("Project file is empty or could not be read");
		Root::Current().Debugger().EndProjectLoadLog(false);
		return false;
	}
	// Windows-authored project files use CRLF. Normalize line endings before
	// parsing so header and final field comparisons behave identically on macOS.
	fileContents.erase(std::remove(fileContents.begin(), fileContents.end(), '\r'), fileContents.end());

	std::istringstream file(fileContents);
	std::string header;
	std::getline(file, header);
	if (header != "AquanactProject")
	{
		Root::Current().Debugger().LogProjectLoadStatus("Invalid project header");
		Root::Current().Debugger().EndProjectLoadLog(false);
		return false;
	}
	Root::Current().Debugger().LogProjectLoadStatus("Parsing project state");
	std::vector<ProjectStateData::PendingLevel> pendingLevels;
	std::vector<ProjectStateData::PendingController> pendingControllers;
	std::vector<ProjectStateData::PendingComponent> pendingComponents;
	std::vector<ProjectStateData::PendingInputAction> pendingInputActions;
	std::vector<std::string> pendingGameGUIAssets;
	std::vector<ProjectStateData::PendingGameGUIAction> pendingGameGUIActions;
	std::string pendingActiveGameGUIAsset;
	std::string pendingGameGUINavigationMode;
	ProjectStateData::RenderStateData renderState;
	std::string startupLevelName;
	const bool loaded = ProjectStateSerializer::LoadLevelState(path, file, pendingLevels, pendingControllers, pendingComponents, pendingInputActions, pendingGameGUIAssets, pendingGameGUIActions, pendingActiveGameGUIAsset, pendingGameGUINavigationMode, renderState, startupLevelName);
	if (loaded) // broken boundary, no longer just I/O
	{
		Root::Current().Debugger().LogProjectLoadStatus("Project state parsed; preparing project assets");
		// Legacy projects stored one global light rig. Give every materialized scene
		// its own initial copy so subsequent edits and scene switches are isolated.
		if (renderState.hasLegacyLighting && !renderState.hasSceneLighting)
		{
			for (ProjectStateData::PendingLevel& pendingLevel : pendingLevels)
			{
				pendingLevel.hasSunLight = true;
				pendingLevel.sunLight = renderState.sunLight;
				pendingLevel.pointLights = renderState.pointLights;
				pendingLevel.spotLights = renderState.spotLights;
			}
		}

		// Establish the project context before materializing assets and applying
		// GUI/render state so every resolver points at this project's assets.
		m_currentProjectPath = path;
		SaveBuildProjectRootPreference(m_currentProjectPath);
		Root::Current().Files().SetRootDirectory(ProjectAssetsDirectory() / "models");
		Root::Current().Spawns().ResetForProject();
		Root::Current().Debugger().LogProjectLoadStatus("Loading spawn data");
		Root::Current().Spawns().LoadProjectState(path);
		Root::Current().Debugger().LogProjectLoadStatus("Creating scenes and entities");
		MaterializePendingLevels(SceneManager, pendingLevels);
		Root::Current().Debugger().LogProjectLoadStatus("Applying scene components");
		SceneManager.ApplyProjectState(pendingLevels, pendingControllers, pendingComponents);
		EnsureMainMenuLevel(SceneManager);
		ApplyStartupLevel(SceneManager, startupLevelName, pendingLevels);
		if (Root::Current().State().IsEditorMode())
		{
			// Open the primary gameplay level directly in the editor so level and
			// gameplay previews are immediately available. Keep MainMenu as a
			// fallback for projects that do not have a Level1 scene.
			if (SceneManager.FindLevel("Level1"))
			{
				SceneManager.SetActiveLevel("Level1");
				SceneManager.SetStartupLevelName("Level1");
			}
			else
			{
				SceneManager.SetActiveLevel("MainMenu");
				SceneManager.SetStartupLevelName("MainMenu");
			}
		}
		if (SceneManager.StartupLevelName().empty())
		{
			SceneManager.SetStartupLevelName("MainMenu");
		}
		Root::Current().Debugger().LogTagged("ProjectLoad", "Applying render state and camera settings");
		// SetTarget establishes the default orbit distance. Apply the saved camera
		// pose and radius afterward so loading cannot replace that saved radius.
		Root::Current().Render().ApplyProjectState(renderState);
		Root::Current().Debugger().LogProjectLoadStatus("Applying editor and GUI state");
		Root::Current().FrontEnd().ApplyProjectState(renderState.editorShowAxis, renderState.editorShowGrid, pendingGameGUIAssets, pendingGameGUIActions, pendingActiveGameGUIAsset, pendingGameGUINavigationMode, renderState.imguiLayout);
		// New project files store camera points per scene. The legacy render-state
		// application above still restores old files, so replace the editor view
		// with the active scene's authored path when the per-scene data exists.
		if (!renderState.sceneCameras.empty())
		{
			if (const Scene* activeScene = SceneManager.ActiveLevel())
			{
				Root::Current().FrontEnd().EditorGUI().CameraPath().Data() = activeScene->CameraSystem().Path();
			}
		}
		Root::Current().Debugger().SetShowLogWindow(renderState.debugShowLogWindow);
		Root::Current().Debugger().SetShowStatsWindow(renderState.debugShowStatsWindow);
		Root::Current().FrontEnd().EditorGUI().SetShowFileExplorer(renderState.showFileExplorer);
		Root::Current().FrontEnd().EditorGUI().SetShowLevelWindow(renderState.showLevelWindow);
		Root::Current().FrontEnd().EditorGUI().SetShowEntityWindow(renderState.showEntityWindow);
		Root::Current().FrontEnd().EditorGUI().SetShowInputMapWindow(renderState.showInputMapWindow);
		Root::Current().FrontEnd().EditorGUI().SetShowCameraWindow(renderState.showCameraWindow);
		Root::Current().Debugger().SetShowGameInputWindow(renderState.showGameInputWindow);
		Root::Current().Debugger().SetShowGameplayDiagnosticsWindow(renderState.showGameplayDiagnosticsWindow);
		Root::Current().Debugger().SetShowAnimationDiagnosticsWindow(renderState.showAnimationDiagnosticsWindow);
		Root::Current().Debugger().SetShowCameraCollisionDebug(renderState.showCameraCollisionDebug);
		Root::Current().Debugger().SetShowPhysicsDiagnosticsWindow(renderState.showPhysicsDiagnosticsWindow);
		Root::Current().FrontEnd().RuntimeGUI().SetShowDiagnosticsWindow(renderState.showGameGUIDiagnosticsWindow);
		Root::Current().Profiler().SetEnabled(renderState.profilerEnabled);
		for (const auto& pendingAction : pendingInputActions)
		{
			std::vector<InputBinding> bindings;
			bindings.reserve(pendingAction.bindings.size());
			for (const auto& bindingData : pendingAction.bindings)
			{
				InputBinding binding;
				binding.type = static_cast<InputBindingType>(bindingData.type);
				binding.code = bindingData.code;
				binding.joystick = bindingData.joystick;
				binding.scale = bindingData.scale;
				binding.vector = bindingData.vector;
				binding.stick = static_cast<InputStick>(bindingData.stick);
				bindings.push_back(binding);
			}
			Root::Current().InputActions().SetBindings(pendingAction.name, std::move(bindings));
		}
		Root::Current().Debugger().EndProjectLoadLog(true);
	}
	else
	{
		Root::Current().Debugger().LogProjectLoadStatus("Project state parser rejected the file");
		Root::Current().Debugger().EndProjectLoadLog(false);
	}
	return loaded;
}

bool ProjectManager::CreateNewProject(const std::filesystem::path& path, SceneManager& SceneManager)
{
	if (!m_fileSystem || path.empty())
	{
		return false;
	}
	std::error_code directoryError;
	for (const char* assetDirectory : { "audio", "gameGUI", "models", "textures" })
	{
		std::filesystem::create_directories(path.parent_path() / "assets" / assetDirectory, directoryError);
		if (directoryError)
		{
			return false;
		}
	}
	for (const std::filesystem::path& projectDirectory : {
		path.parent_path() / "src" / "Game",
		path.parent_path() / "include" / "Game",
		path.parent_path() / "generated" })
	{
		std::filesystem::create_directories(projectDirectory, directoryError);
		if (directoryError)
		{
			return false;
		}
	}

	// Switch the asset context before resetting project-owned UI. Otherwise a
	// project created from the startup selector can inherit GUI data discovered
	// before any project was selected.
	const std::filesystem::path previousProjectPath = m_currentProjectPath;
	m_currentProjectPath = path;
	GameGUIManager& runtimeGUI = Root::Current().FrontEnd().RuntimeGUI();
	runtimeGUI.ReloadAssetsFromDisk();
	runtimeGUI.SetSceneAssets({});
	runtimeGUI.SetMenuNavigationMode(GameGUIMenuNavigationMode::Pointer);
	if (Root::Current().State().IsEditorMode())
	{
		Root::Current().FrontEnd().Creator().ReloadAssetsFromDisk();
	}

	SceneManager.Clear();
	Root::Current().Spawns().ResetForProject();
	EnsureMainMenuLevel(SceneManager);
	SceneManager.SetStartupLevelName("MainMenu");
	Root::Current().InputActions().ResetToDefaults();
	Root::Current().Render().ResetForNewProject();
	Root::Current().FrontEnd().EditorGUI().CameraPath().Clear();
	Root::Current().FrontEnd().EditorGUI().SetShowCameraPath(false);

	const bool saved = SaveProject(path, SceneManager);
	if (saved)
	{
		Root::Current().Files().SetRootDirectory(ProjectAssetsDirectory() / "models");
	}
	else
	{
		m_currentProjectPath = previousProjectPath;
	}
	return saved;
}





