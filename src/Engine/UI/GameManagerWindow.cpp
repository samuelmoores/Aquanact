#include "Engine/UI/GameManagerWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Scene.h"
#include "Game/GameManager.h"

#include <imgui.h>

void GameManagerWindow::Draw(Scene* scene, bool& open) const
{
	if (!scene)
		return;

	GameManager* manager = nullptr;
	for (const auto& object : scene->Objects())
	{
		if (object && (manager = object->GetComponent<GameManager>()))
			break;
	}
	if (!manager)
		return;

	if (ImGui::Begin("GameManager", &open, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		ImGui::Text("Overlap Count: %d", manager->OverlapCount());
		ImGui::Text("Elapsed Time: %.2f seconds", manager->ElapsedSeconds());
	}
	ImGui::End();
}
