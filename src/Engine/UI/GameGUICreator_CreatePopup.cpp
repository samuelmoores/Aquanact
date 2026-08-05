#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>

namespace {
	Entity* FindEntity(Scene* scene, const std::string& name)
	{
		if (!scene)
		{
			return nullptr;
		}
		for (const std::unique_ptr<Entity>& entity : scene->Entities())
		{
			if (entity && entity->Name() == name)
			{
				return entity.get();
			}
		}
		return nullptr;
	}
}

void GameGUICreator::DrawCreateWidgetPopup()
{
	if (m_showCreateWidgetPopup)
	{
		ImGui::OpenPopup("Create Widget");
		m_showCreateWidgetPopup = false;
	}

	if (!ImGui::BeginPopupModal("Create Widget", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	const char* widgetKind = "Create a button widget:";
	if (m_newWidgetIsPanel)
	{
		widgetKind = "Create a panel widget:";
	}
	else if (m_newWidgetIsProgressBar)
	{
		widgetKind = "Create a progress bar widget:";
	}
	else if (m_newWidgetIsImage)
	{
		widgetKind = "Create an image widget:";
	}

	ImGui::TextUnformatted(widgetKind);
	ImGui::InputText("Name", m_newWidgetName, sizeof(m_newWidgetName));

	// Panels only need a name; the texture field is for content widgets.
	if (!m_newWidgetIsPanel)
	{
		std::string texturePath = m_newWidgetTexture;
		ImGui::TextUnformatted("Texture");
		if (GameGUICreatorHelpers::DrawTextureCombo("##NewWidgetTexture", texturePath, false, "<No Texture>"))
		{
			std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "%s", texturePath.c_str());
		}
	}

	// Panels, images, and progress bars stop at the shared fields above.
	// Only button widgets expose binding controls in this popup.
	if (!m_newWidgetIsPanel && !m_newWidgetIsImage && !m_newWidgetIsProgressBar)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Binding");
		GameGUIActionType action = m_newWidgetAction;
		const char* bindingLabel = action == GameGUIActionType::NewGame ? "New Game" : "None";
		if (ImGui::BeginCombo("Action", bindingLabel))
		{
			const GameGUIActionType options[] = { GameGUIActionType::None, GameGUIActionType::NewGame };
			for (GameGUIActionType option : options)
			{
				const bool selected = option == action;
				if (ImGui::Selectable(option == GameGUIActionType::NewGame ? "New Game" : "None", selected))
				{
					action = option;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		m_newWidgetAction = action;

		if (m_newWidgetAction == GameGUIActionType::NewGame)
		{
			SceneManager& sceneManager = Root::Current().Scenes();
			const std::vector<std::string> levelNames = sceneManager.SceneNames(SceneManager::SceneKind::Level);
			const char* launchLabel = m_newWidgetLaunchLevel.empty() ? "<Select Level>" : m_newWidgetLaunchLevel.c_str();
			if (ImGui::BeginCombo("Launch Level", launchLabel))
			{
				for (const std::string& levelName : levelNames)
				{
					const bool selected = m_newWidgetLaunchLevel == levelName;
					if (ImGui::Selectable(levelName.c_str(), selected))
					{
						m_newWidgetLaunchLevel = levelName;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			if (levelNames.empty())
			{
				ImGui::TextDisabled("No gameplay scenes are available.");
			}
		}
		else
		{
			m_newWidgetLaunchLevel.clear();
		}
	}

	if (ImGui::Button("Create"))
	{
		// Dispatch to the specific creation path based on the popup mode.
		if (m_newWidgetIsPanel)
		{
			AddPanelWidget();
		}
		else if (m_newWidgetIsProgressBar)
		{
			AddProgressBarWidget();
		}
		else if (m_newWidgetIsImage)
		{
			AddImageWidget();
		}
		else
		{
			AddButtonWidget();
		}

		SyncRuntimePreview();
		// Keep the log message aligned with the widget type that was just created.
		const char* logMessage = m_newWidgetIsPanel ? "Create Panel requested"
			: m_newWidgetIsProgressBar ? "Create Progress Bar requested"
			: m_newWidgetIsImage ? "Create Image requested"
			: "Create Button requested";
		Root::Current().Debugger().LogMessage(logMessage);
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}





