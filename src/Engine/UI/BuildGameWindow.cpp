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
#include <cstdlib>
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
	if (ImGui::Button("Package for Web"))
	{
		m_requestedWebBuild = true;
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

	if (m_requestedWebBuild)
	{
		m_requestedWebBuild = false;
		const std::filesystem::path sourceRoot = std::filesystem::absolute(SourceRoot());
		const std::filesystem::path outputRoot =
			std::filesystem::absolute(std::filesystem::path(m_buildPath)) / "web";
		const std::filesystem::path projectFile = Root::Current().Projects().CurrentProjectPath();
		const std::filesystem::path scriptPath = sourceRoot / "scripts" / "package_web.ps1";
		Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
		if (!std::filesystem::exists(scriptPath))
		{
			m_statusMessage = "Web package failed: scripts/package_web.ps1 is missing.";
		}
		else if (projectFile.empty() || !Root::Current().Projects().SaveProject(projectFile, Root::Current().Scenes()))
		{
			m_statusMessage = "Web package failed: could not save project.";
		}
		else
		{
			const std::string command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" +
				scriptPath.string() + "\" -SourceRoot \"" + sourceRoot.string() +
				"\" -ProjectFile \"" + projectFile.string() + "\" -OutputRoot \"" +
				outputRoot.string() + "\"";
			const int exitCode = std::system(command.c_str());
			m_statusMessage = exitCode == 0
				? "Web package created: " + outputRoot.string() +
					" (ZIP: " + outputRoot.parent_path().append("aquanact-web.zip").string() + ")"
				: "Web package failed. Check the Emscripten output above.";
		}
	}

	EngineGuiWidgets::StatusMessage(m_statusMessage);
}
