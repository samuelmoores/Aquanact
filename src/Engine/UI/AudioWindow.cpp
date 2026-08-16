#include "Engine/UI/AudioWindow.h"

#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <filesystem>
#include <utility>

void AudioWindow::Draw(SceneManager& sceneManager, ProjectManager& projectManager, bool& open)
{
	if (!open) return;

	EngineGuiWidgets::WindowScope window("Audio", &open);
	if (window)
	{
		Scene* scene = sceneManager.ActiveLevel();
		if (scene)
		{
			ImGui::Text("Level Music");
			const EngineGuiWidgets::AssetFilePickerOptions musicOptions{
				EngineGuiWidgets::SourceAssetDirectory("assets/audio/music"),
				"audio/music/",
				{ ".wav", ".mp3", ".ogg", ".flac" },
				false,
				"<Select music>" };
			std::string selectedMusicPath = scene->MusicPath();
			if (EngineGuiWidgets::AssetFileCombo("Music", selectedMusicPath, musicOptions))
			{
				scene->SetMusicPath(std::move(selectedMusicPath));
				const std::filesystem::path projectPath = projectManager.CurrentProjectPath();
				if (!projectPath.empty()) projectManager.SaveProject(projectPath, sceneManager);
			}

			float volume = scene->MusicVolume();
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::SliderFloat("Volume", &volume, 0.0f, 100.0f))
			{
				scene->SetMusicVolume(volume);
				const std::filesystem::path projectPath = projectManager.CurrentProjectPath();
				if (!projectPath.empty()) projectManager.SaveProject(projectPath, sceneManager);
			}
		}
		else
		{
			ImGui::TextUnformatted("No active scene.");
		}
	}
}
