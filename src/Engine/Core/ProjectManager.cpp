#include "Engine/Core/ProjectManager.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/FileManager.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectStateSerializer.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/InputManager.h"
#include <fstream>
#include <imgui.h>
#include <sstream>
#include <vector>

namespace {
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
			if (pendingLevel.isMainMenu)
			{
				SceneManager.SetSceneKind(pendingLevel.name, SceneManager::SceneKind::Cutscene);
			}

			for (const auto& pendingObject : pendingLevel.objects)
			{
				Root::Current().Debugger().LogTagged("ProjectLoad", "Creating object from source: " + pendingObject.sourcePath.string() + " in scene: " + pendingLevel.name);
				// Project component records are authoritative. Imported models still receive
				// their default components through Entity's default constructor behavior.
				auto object = std::make_unique<Entity>(pendingObject.sourcePath.string().c_str(), false);
				if (pendingObject.id != 0)
				{
					object->SetId(pendingObject.id);
				}
				object->Translate(pendingObject.position);
				object->SetRotation(pendingObject.rotation);
				object->SetScale(pendingObject.scale);
				object->SetIgnoreCameraCollision(pendingObject.ignoreCameraCollision);
				object->SetBlocksCameraView(pendingObject.blocksCameraView);
				object->SetShowPhysicsBoundingBox(pendingObject.showPhysicsBoundingBox);
				object->SetPhysicsColliderShape(pendingObject.physicsColliderShape == 1
					? PhysicsColliderShape::Capsule
					: pendingObject.physicsColliderShape == 2 ? PhysicsColliderShape::Convex : PhysicsColliderShape::Box);
				object->SetDefaultPosition(object->Position());
				object->SetDefaultRotation(object->Rotation());
				Scene->AddObject(std::move(object));
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
		Root::Current().Files().SetRootDirectory(ProjectAssetsDirectory() / "models");
	}
	return written;
}

bool ProjectManager::LoadProject(const std::filesystem::path& path, SceneManager& SceneManager)
{
	if (!m_fileSystem)
	{
		return false;
	}

	const std::string fileContents = m_fileSystem->ReadTextFile(path);
	if (fileContents.empty())
	{
		return false;
	}

	std::istringstream file(fileContents);
	std::string header;
	std::getline(file, header);
	if (header != "AquanactProject")
	{
		return false;
	}
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
		// Establish the project context before materializing assets and applying
		// GUI/render state so every resolver points at this project's assets.
		m_currentProjectPath = path;
		Root::Current().Files().SetRootDirectory(ProjectAssetsDirectory() / "models");
		MaterializePendingLevels(SceneManager, pendingLevels);
		SceneManager.ApplyProjectState(pendingLevels, pendingControllers, pendingComponents);
		EnsureMainMenuLevel(SceneManager);
		ApplyStartupLevel(SceneManager, startupLevelName, pendingLevels);
		if (Root::Current().State().IsEditorMode())
		{
			// Opening a project always begins at its frontend. New Game destinations
			// are stored per button and must not determine the editor's opening scene.
			SceneManager.SetActiveLevel("MainMenu");
			SceneManager.SetStartupLevelName("MainMenu");
		}
		if (SceneManager.StartupLevelName().empty())
		{
			SceneManager.SetStartupLevelName("MainMenu");
		}
		Root::Current().Debugger().LogTagged("ProjectLoad", "Applying render state and camera settings");
		// SetTarget establishes the default orbit distance. Apply the saved camera
		// pose and radius afterward so loading cannot replace that saved radius.
		Root::Current().Render().ApplyProjectState(renderState);
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
		Root::Current().FrontEnd().EditorGUI().SetShowLightingWindow(renderState.showLightingWindow);
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





