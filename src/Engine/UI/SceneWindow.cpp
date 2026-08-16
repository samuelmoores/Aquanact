#include "Engine/UI/SceneWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <memory>
#include <string>
#include <vector>
#include <cctype>

namespace
{
	std::string NormalizeSceneName(const std::string& input)
	{
		std::string output;
		output.reserve(input.size());
		bool capitalizeNext = true;
		for (unsigned char ch : input)
		{
			if (std::isalnum(ch))
			{
				output.push_back(capitalizeNext ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch));
				capitalizeNext = false;
			}
			else
			{
				capitalizeNext = true;
			}
		}
		return output;
	}
}

void SceneWindow::DrawNewLevelPopup(SceneManager& sceneManager, bool& requested)
{
	if (requested)
	{
		m_newLevelStatusMessage.clear();
	}
	EngineGuiWidgets::ConsumePopupRequest("New Scene##AquanactNewLevel", requested);

	EngineGuiWidgets::ModalScope popup("New Scene##AquanactNewLevel");
	if (!popup)
		return;

	ImGui::TextUnformatted("Create a new scene:");
	ImGui::InputText("Name", m_newLevelName, sizeof(m_newLevelName));
	if (ImGui::Button("Create"))
	{
		const std::string levelName = NormalizeSceneName(m_newLevelName);
		if (levelName.empty())
			m_newLevelStatusMessage = "Enter a valid scene name.";
		else if (sceneManager.FindLevel(levelName))
			m_newLevelStatusMessage = "Scene already exists.";
		else if (sceneManager.CreateLevel(levelName))
		{
			sceneManager.SetActiveLevel(levelName);
			m_newLevelStatusMessage = "Created scene " + levelName + ".";
		}
		else
			m_newLevelStatusMessage = "Failed to create scene.";
	}
	ImGui::SameLine();
	EngineGuiWidgets::CloseButton();
	EngineGuiWidgets::StatusMessage(m_newLevelStatusMessage);
}

void SceneWindow::Draw(
	const EngineGuiFrameContext& context,
	bool& open,
	bool& entityWindowOpen) const
{
	if (!context.sceneManager || !context.selection)
		return;
	SceneManager& sceneManager = *context.sceneManager;
	unsigned int& selectedEntityId = context.selection->entityId;
	const Scene* activeScene = sceneManager.ActiveLevel();
	const std::string title = activeScene ? activeScene->Name() : "Scene";
	if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		if (activeScene)
		{
			const std::vector<std::unique_ptr<Entity>>& objects = activeScene->Objects();
			for (std::size_t i = 0; i < objects.size(); ++i)
			{
				const std::unique_ptr<Entity>& object = objects[i];
				const std::string label = object ? object->Name() : "<null>";
				const std::string visibleLabel = label.empty() ? "<unnamed>" : label;
				const std::string selectableId = visibleLabel + "##LevelObject" + std::to_string(i);
				if (ImGui::Selectable(selectableId.c_str(), object && selectedEntityId == object->Id()))
				{
					selectedEntityId = object ? object->Id() : 0;
					entityWindowOpen = true;
				}
			}
			if (objects.empty())
			{
				ImGui::TextUnformatted("No entities in this Scene.");
			}
		}
		else
		{
			ImGui::TextUnformatted("No active Scene selected.");
		}
	}
	ImGui::End();
}
