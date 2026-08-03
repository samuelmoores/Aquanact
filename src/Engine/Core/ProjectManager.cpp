#include "Engine/Core/ProjectManager.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectStateSerializer.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/RenderManager.h"
#include <fstream>
#include <imgui.h>
#include <cstring>
#include <sstream>
#include <vector>

namespace {
	void AppendCurrentCameraState(std::string& contents)
	{
		const GameCamera& gameCamera = Root::Current().Render().GetGameCamera();
		const glm::vec3 gameCameraPosition = gameCamera.GetPosition();
		const glm::vec3 gameCameraFacing = gameCamera.GetFacing();
		contents += "gamecamera;";
		contents += std::to_string(gameCameraPosition.x) + ";" + std::to_string(gameCameraPosition.y) + ";" + std::to_string(gameCameraPosition.z) + ";";
		contents += std::to_string(gameCameraFacing.x) + ";" + std::to_string(gameCameraFacing.y) + ";" + std::to_string(gameCameraFacing.z) + ";";
		contents += std::to_string(gameCamera.Radius()) + ";";
		contents += std::to_string(gameCamera.Yaw()) + ";";
		contents += std::to_string(gameCamera.Pitch()) + ";";
		contents += ProjectStateSerializer::EscapeField(gameCamera.TargetName()) + ";";
		contents += (gameCamera.TargetName().empty() ? "0" : "1");
		contents += "\n";
	}

	void AppendImguiLayoutState(std::string& contents)
	{
		if (const char* imguiIniData = ImGui::SaveIniSettingsToMemory())
		{
			contents += "imguilayout;";
			contents += ProjectStateSerializer::HexEncode(imguiIniData, std::strlen(imguiIniData));
			contents += "\n";
		}
	}

	void AppendProjectStateSnapshot(std::string& contents, const std::filesystem::path& path, const SceneManager& SceneManager)
	{
		ProjectStateSerializer::AppendLevelState(contents, path, SceneManager);
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
				object->SetShowPhysicsBoundingBox(pendingObject.showPhysicsBoundingBox);
				object->SetPhysicsColliderShape(pendingObject.physicsColliderShape == 1
					? PhysicsColliderShape::Capsule
					: pendingObject.physicsColliderShape == 2 ? PhysicsColliderShape::Convex : PhysicsColliderShape::Box);
				object->SetDefaultPosition(object->Position());
				object->SetDefaultRotation(object->Rotation());
				Scene->AddObject(std::move(object));
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

	void RestoreGameCameraTarget(SceneManager& SceneManager, const ProjectStateData::RenderStateData& renderState)
	{
		GameCamera& gameCamera = Root::Current().Render().GetGameCamera();
		gameCamera.SetTarget(nullptr);
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

			gameCamera.SetTarget(object.get());
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
	const int projectVersion = 18;
	if (header != "AquanactProject" && header != "AquanactProject 18")
	{
		return false;
	}
	std::vector<ProjectStateData::PendingLevel> pendingLevels;
	std::vector<ProjectStateData::PendingController> pendingControllers;
	std::vector<ProjectStateData::PendingComponent> pendingComponents;
	std::vector<std::string> pendingGameGUIAssets;
	std::string pendingActiveGameGUIAsset;
	std::string pendingGameGUINavigationMode;
	ProjectStateData::RenderStateData renderState;
	std::string startupLevelName;
	const bool loaded = ProjectStateSerializer::LoadLevelState(path, file, projectVersion, pendingLevels, pendingControllers, pendingComponents, pendingGameGUIAssets, pendingActiveGameGUIAsset, pendingGameGUINavigationMode, renderState, startupLevelName);
	if (loaded) // broken boundary, no longer just I/O
	{
		MaterializePendingLevels(SceneManager, pendingLevels);
		SceneManager.ApplyProjectState(pendingLevels, pendingControllers, pendingComponents);
		EnsureMainMenuLevel(SceneManager);
		ApplyStartupLevel(SceneManager, startupLevelName, pendingLevels);
		if (Root::Current().State().IsEditorMode())
		{
			const auto gameplayLevels = SceneManager.SceneNames(SceneManager::SceneKind::Level);
			if (!gameplayLevels.empty())
			{
				SceneManager.SetActiveLevel(gameplayLevels.front());
				SceneManager.SetStartupLevelName(gameplayLevels.front());
			}
		}
		if (SceneManager.StartupLevelName().empty())
		{
			SceneManager.SetStartupLevelName("MainMenu");
		}
		Root::Current().Debugger().LogTagged("ProjectLoad", "Applying render state and camera settings");
		// SetTarget establishes the default orbit distance. Apply the saved camera
		// pose and radius afterward so loading cannot replace that saved radius.
		RestoreGameCameraTarget(SceneManager, renderState);
		Root::Current().Render().ApplyProjectState(renderState);
		Root::Current().FrontEnd().ApplyProjectState(renderState.editorShowAxis, renderState.editorShowGrid, pendingGameGUIAssets, pendingActiveGameGUIAsset, pendingGameGUINavigationMode, renderState.imguiLayout);
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
		m_currentProjectPath = path;
	}
	return loaded;
}





