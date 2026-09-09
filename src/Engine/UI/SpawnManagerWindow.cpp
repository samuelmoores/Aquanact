#include "Engine/UI/SpawnManagerWindow.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/SpawnManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

void SpawnManagerWindow::Draw(SceneManager& scenes, bool& open)
{
	if (!open)
		return;
	if (!ImGui::Begin("SpawnManager", &open))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Spawn Instance");
	EngineGuiWidgets::Vector3Editor("Position", m_position, 0.1f);
	EngineGuiWidgets::Vector3Editor("Rotation", m_rotation, 0.01f);
	ImGui::Separator();

	Scene* scene = scenes.ActiveLevel();
	if (!scene)
	{
		ImGui::TextDisabled("No active scene.");
	}
	else if (Root::Current().Spawns().Definitions().empty())
	{
		ImGui::TextDisabled("No instance definitions created.");
	}
	else
	{
		for (const auto& entry : Root::Current().Spawns().Definitions())
		{
			ImGui::PushID(entry.first.c_str());
			ImGui::TextUnformatted(entry.first.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Spawn"))
				Root::Current().Spawns().SpawnInstance(*scene, entry.first, m_position, m_rotation);
			ImGui::PopID();
		}
	}
	ImGui::End();
}
