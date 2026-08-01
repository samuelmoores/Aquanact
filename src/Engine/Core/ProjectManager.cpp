#include "Engine/Core/ProjectManager.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectStateSerializer.h"
#include "Engine/Core/RenderManager.h"
#include <fstream>
#include <imgui.h>
#include <cstring>
#include <sstream>
#include <vector>

namespace {
	void AppendCurrentCameraState(std::string& contents)
	{
		const glm::vec3 gameCameraPosition = Root::Current().Render().GetGameCamera().GetPosition();
		const glm::vec3 gameCameraFacing = Root::Current().Render().GetGameCamera().GetFacing();
		contents += "gamecamera;";
		contents += std::to_string(gameCameraPosition.x) + ";" + std::to_string(gameCameraPosition.y) + ";" + std::to_string(gameCameraPosition.z) + ";";
		contents += std::to_string(gameCameraFacing.x) + ";" + std::to_string(gameCameraFacing.y) + ";" + std::to_string(gameCameraFacing.z) + "\n";
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
		AppendCurrentCameraState(contents);
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
				auto object = std::make_unique<Entity>(pendingObject.sourcePath.string().c_str());
				object->Translate(pendingObject.position);
				object->SetRotation(pendingObject.rotation);
				object->SetScale(pendingObject.scale);
				object->SetIgnoreCameraCollision(pendingObject.ignoreCameraCollision);
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

		std::string contents = "AquanactProject 16\n";
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
	int projectVersion = 0;
	if (header == "AquanactProject 10")
	{
		projectVersion = 10;
	}
	else if (header == "AquanactProject 11")
	{
		projectVersion = 11;
	}
	else if (header == "AquanactProject 12")
	{
		projectVersion = 12;
	}
	else if (header == "AquanactProject 13")
	{
		projectVersion = 13;
	}
	else if (header == "AquanactProject 14")
	{
		projectVersion = 14;
	}
	else if (header == "AquanactProject 15")
	{
		projectVersion = 15;
	}
	else if (header == "AquanactProject 16")
	{
		projectVersion = 16;
	}
	else
	{
		return false;
	}
	std::vector<ProjectStateData::PendingLevel> pendingLevels;
	std::vector<ProjectStateData::PendingController> pendingControllers;
	std::vector<ProjectStateData::PendingComponent> pendingComponents;
	std::vector<std::string> pendingGameGUIAssets;
	std::string pendingActiveGameGUIAsset;
	ProjectStateData::RenderStateData renderState;
	std::string startupLevelName;
	const bool loaded = ProjectStateSerializer::LoadLevelState(path, file, projectVersion, pendingLevels, pendingControllers, pendingComponents, pendingGameGUIAssets, pendingActiveGameGUIAsset, renderState, startupLevelName);
	if (loaded) // broken boundary, no longer just I/O
	{
		MaterializePendingLevels(SceneManager, pendingLevels);
		SceneManager.ApplyProjectState(pendingLevels, pendingControllers, pendingComponents);
		EnsureMainMenuLevel(SceneManager);
		ApplyStartupLevel(SceneManager, startupLevelName, pendingLevels);
		if (SceneManager.StartupLevelName().empty())
		{
			SceneManager.SetStartupLevelName("MainMenu");
		}
		Root::Current().Render().ApplyProjectState(renderState);
		Root::Current().FrontEnd().ApplyProjectState(renderState.editorShowAxis, renderState.editorShowGrid, pendingGameGUIAssets, pendingActiveGameGUIAsset, renderState.imguiLayout);
		Root::Current().Debugger().SetShowLogWindow(renderState.debugShowLogWindow);
		Root::Current().Debugger().SetShowStatsWindow(renderState.debugShowStatsWindow);
		m_currentProjectPath = path;
	}
	return loaded;
}





