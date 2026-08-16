#include "Engine/UI/BuildGameWindow.h"

#include "Engine/Core/AquanactBuildSystem.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

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
		const std::filesystem::path projectFile = sourceRoot / "assets" / "projects" / "project.aqua";
		const std::filesystem::path executablePath = std::filesystem::current_path() / "AquanactGame.exe";
		const bool succeeded = buildSystem.Build(sourceRoot, outputRoot, projectFile, executablePath);
		m_statusMessage = succeeded ? "Build succeeded." : "Build failed.";
	}

	EngineGuiWidgets::StatusMessage(m_statusMessage);
}
