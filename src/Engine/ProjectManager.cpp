#include "Engine/ProjectManager.h"

#include "Engine/Globals.h"
#include "Engine/FrontEndManager.h"
#include "Engine/FileSystem.h"
#include "Engine/LevelManager.h"
#include "Engine/ProjectStateSerializer.h"
#include "Engine/RenderManager.h"

#include <fstream>
#include <imgui.h>
#include <cstring>
#include <sstream>
#include <vector>

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
	ProjectStateSerializer::AppendLevelState(contents, path, levelManager);

	const glm::vec3 gameCameraPosition = gRenderManager.GetGameCamera().GetPosition();
	const glm::vec3 gameCameraFacing = gRenderManager.GetGameCamera().GetFacing();
	contents += "gamecamera;";
	contents += std::to_string(gameCameraPosition.x) + ";" + std::to_string(gameCameraPosition.y) + ";" + std::to_string(gameCameraPosition.z) + ";";
	contents += std::to_string(gameCameraFacing.x) + ";" + std::to_string(gameCameraFacing.y) + ";" + std::to_string(gameCameraFacing.z) + "\n";

	ProjectStateSerializer::AppendRenderState(contents, gFrontEndManager, gRenderManager);

	levelManager.AppendProjectState(contents);

	gFrontEndManager.RuntimeGUI().AppendProjectState(contents);

	if (const char* imguiIniData = ImGui::SaveIniSettingsToMemory())
	{
		contents += "imguilayout;";
		contents += ProjectStateSerializer::HexEncode(imguiIniData, std::strlen(imguiIniData));
		contents += "\n";
	}

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
	std::vector<std::string> pendingGameGUIAssets;
	std::string pendingActiveGameGUIAsset;
	std::string pendingImguiLayout;
	gRenderManager.Lights().PointLights().clear();
	const bool loaded = ProjectStateSerializer::LoadLevelState(path, file, projectVersion, levelManager, gFrontEndManager, gRenderManager, pendingGameGUIAssets, pendingActiveGameGUIAsset, pendingImguiLayout);
	if (loaded)
	{
		m_currentProjectPath = path;
	}
	return loaded;
}


