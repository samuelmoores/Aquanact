#include "Engine/Core/ProjectManager.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/LevelManager.h"
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

	void AppendProjectStateSnapshot(std::string& contents, const std::filesystem::path& path, const LevelManager& levelManager)
	{
		ProjectStateSerializer::AppendLevelState(contents, path, levelManager);
		AppendCurrentCameraState(contents);
		ProjectStateSerializer::AppendRenderState(contents, Root::Current().FrontEnd(), Root::Current().Render());
		levelManager.AppendProjectState(contents);
		Root::Current().FrontEnd().RuntimeGUI().AppendProjectState(contents);
		AppendImguiLayoutState(contents);
	}

	void MaterializePendingLevels(LevelManager& levelManager, const std::vector<ProjectStateData::PendingLevel>& pendingLevels)
	{
		levelManager.Clear();
		for (const auto& pendingLevel : pendingLevels)
		{
			Level* level = levelManager.CreateLevel(pendingLevel.name);
			if (!level)
			{
				continue;
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
				level->AddObject(std::move(object));
			}
		}
	}

	void ApplyStartupLevel(LevelManager& levelManager, const std::string& startupLevelName, const std::vector<ProjectStateData::PendingLevel>& pendingLevels)
	{
		if (!pendingLevels.empty())
		{
			const Level* activeLevel = nullptr;
			for (const auto& pendingLevel : pendingLevels)
			{
				if (pendingLevel.active)
				{
					activeLevel = levelManager.FindLevel(pendingLevel.name);
					break;
				}
			}
			if (!activeLevel)
			{
				activeLevel = levelManager.Levels().front().get();
			}
			levelManager.SetActiveLevel(activeLevel->Name());
		}

		if (!startupLevelName.empty())
		{
			levelManager.ApplyProjectState(startupLevelName);
		}
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

bool ProjectManager::SaveProject(const std::filesystem::path& path, const LevelManager& levelManager)
{
	if (!m_fileSystem)
	{
		return false;
	}

	std::string contents = "AquanactProject 13\n";
	AppendProjectStateSnapshot(contents, path, levelManager);

	const bool written = m_fileSystem->WriteTextFile(path, contents);
	if (written)
	{
		m_currentProjectPath = path;
	}
	return written;
}

bool ProjectManager::LoadProject(const std::filesystem::path& path, LevelManager& levelManager)
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
		MaterializePendingLevels(levelManager, pendingLevels);
		levelManager.ApplyProjectState(pendingLevels, pendingControllers, pendingComponents);
		ApplyStartupLevel(levelManager, startupLevelName, pendingLevels);
		Root::Current().Render().ApplyProjectState(renderState);
		Root::Current().FrontEnd().ApplyProjectState(renderState.editorShowAxis, renderState.editorShowGrid, pendingGameGUIAssets, pendingActiveGameGUIAsset, renderState.imguiLayout);
		m_currentProjectPath = path;
	}
	return loaded;
}


