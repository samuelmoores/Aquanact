#include "Engine/UI/BuildGameWindow.h"

#include "Engine/Core/AquanactBuildSystem.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <cstdio>
#include <filesystem>

namespace
{
	std::filesystem::path SourceRoot()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT);
#else
		return std::filesystem::current_path();
#endif
	}
}

void BuildGameWindow::Draw(bool& popupRequested)
{
	EngineGuiWidgets::ConsumePopupRequest("Build Game##AquanactBuildGame", popupRequested);
	EngineGuiWidgets::ModalScope popup("Build Game##AquanactBuildGame");
	if (!popup) return;
	if (!m_initializedPath)
	{
		const std::filesystem::path defaultOutput = SourceRoot().parent_path() / "Aquanact-package";
		std::snprintf(m_buildPath, sizeof(m_buildPath), "%s", defaultOutput.string().c_str());
		m_initializedPath = true;
	}

	ImGui::TextUnformatted("Build the packaged game to this folder:");
	ImGui::InputText("Output", m_buildPath, sizeof(m_buildPath));
	if (ImGui::Button("Build"))
	{
		m_requestedBuild = true;
	}
	ImGui::SameLine();
	EngineGuiWidgets::CloseButton();

	if (m_requestedBuild)
	{
		m_requestedBuild = false;
		AquanactBuildSystem buildSystem;
		const std::filesystem::path sourceRoot = SourceRoot();
		const std::filesystem::path outputRoot = std::filesystem::path(m_buildPath);
		const std::filesystem::path projectFile = Root::Current().Projects().CurrentProjectPath();
		const std::filesystem::path executablePath =
			Root::Current().FileSystemRef().ExecutableDirectory() / "AquanactGame.exe";
		Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
		if (projectFile.empty() || !Root::Current().Projects().SaveProject(projectFile, Root::Current().Scenes()))
		{
			m_statusMessage = "Build failed: could not save project.";
		}
		else
		{
			const AquanactBuildSystem::Result result = buildSystem.Build(
				sourceRoot, outputRoot, projectFile, executablePath);
			m_statusMessage = result.message;
		}
	}

	EngineGuiWidgets::StatusMessage(m_statusMessage);
}
