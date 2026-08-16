#include "Engine/UI/FileExplorerWindow.h"

#include "Engine/Core/FileManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <filesystem>
#include <string>

void FileExplorerWindow::Draw(FileManager& fileManager, bool& open)
{
	if (!open) return;

	EngineGuiWidgets::WindowScope window(
		"File Explorer",
		&open,
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
	if (!window) return;

	if (ImGui::Button("Models"))
	{
		fileManager.SetRootDirectory("C:/dev/Aquanact/assets/models");
	}

	if (fileManager.CanImportSelection() && ImGui::Button("Import Selected"))
	{
		fileManager.ImportSelected();
	}

	ImGui::Separator();
	for (const std::filesystem::directory_entry& entry : fileManager.Entries())
	{
		if (entry.is_directory()) continue;

		const std::filesystem::path entryPath = entry.path();
		const std::string label = entryPath.filename().string();
		const bool selected = fileManager.HasSelection() && fileManager.SelectedPath() == entryPath;
		if (ImGui::Selectable(label.c_str(), selected))
		{
			fileManager.SelectPath(entryPath);
		}
	}
}
