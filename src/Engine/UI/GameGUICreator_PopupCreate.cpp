#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <algorithm>

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
	// Opening the popup is a two-step ImGui flow:
	// 1. request the popup once when the flag is set
	// 2. draw the popup every frame while ImGui keeps it open
	if (m_showCreateWidgetPopup)
	{
		ImGui::OpenPopup("Create Widget");
		m_showCreateWidgetPopup = false;
	}

	if (!ImGui::BeginPopupModal("Create Widget", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	switch (m_newWidgetType)
	{
	case NewWidgetType::Panel:
		DrawCreatePanelPopup();
		break;
	case NewWidgetType::ProgressBar:
		DrawCreateProgressBarPopup();
		break;
	case NewWidgetType::Image:
		DrawCreateImagePopup();
		break;
	case NewWidgetType::Text:
		DrawCreateTextPopup();
		break;
	case NewWidgetType::Button:
	default:
		DrawCreateButtonPopup();
		break;
	}

	ImGui::EndPopup();
}

void GameGUICreator::OpenCreateWidgetPopup(NewWidgetType type)
{
	m_newWidgetType = type;
	m_showCreateWidgetPopup = true;
	m_newWidgetAction = GameGUIActionType::None;
	m_newWidgetLaunchLevel.clear();
	m_newWidgetTargetPanel.clear();
	m_newWidgetName[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	if (type == NewWidgetType::Panel)
	{
		m_newWidgetParentPanel.clear();
		m_newPanelVisible = std::none_of(CurrentGameGUI().widgets.begin(), CurrentGameGUI().widgets.end(), [](const GameGUIWidgetDef& widget)
		{
			return widget.type == "Panel";
		});
	}
	else
	{
		RefreshActiveEditingPanel();
		m_newWidgetParentPanel = m_activeEditingPanel;
	}
}

void GameGUICreator::DrawCreateWidgetPopupHeader(const char* title)
{
	ImGui::TextUnformatted(title);
	DrawCreateWidgetNameField();
}

void GameGUICreator::DrawCreateWidgetPopupFooter()
{
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();
	}
}

void GameGUICreator::DrawCreateWidgetNameField()
{
	ImGui::InputText("Name", m_newWidgetName, sizeof(m_newWidgetName));
}

void GameGUICreator::DrawCreateWidgetTextureField()
{
	// Texture picking is reused by image and progress bar creation.
	// The only widget-specific decision here is whether the field appears.
	std::string texturePath = m_newWidgetTexture;
	ImGui::TextUnformatted("Texture");
	if (GameGUICreatorHelpers::DrawTextureCombo("##NewWidgetTexture", texturePath, false, "<No Texture>"))
	{
		std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "%s", texturePath.c_str());
	}
}

void GameGUICreator::DrawCreateActionField()
{
	// Button creation is the only popup path that currently needs action binding.
	// If more widget types gain launch behavior, this block should become shared.
	GameGUIActionType action = m_newWidgetAction;
	const char* bindingLabel = GameGUICreatorHelpers::ActionLabel(action);
	if (ImGui::BeginCombo("Action", bindingLabel))
	{
		const GameGUIActionType options[] = {
			GameGUIActionType::None,
			GameGUIActionType::NewGame,
			GameGUIActionType::Pause,
			GameGUIActionType::Resume,
			GameGUIActionType::SubPanel,
		};
		for (GameGUIActionType option : options)
		{
			const bool selected = option == action;
			if (ImGui::Selectable(GameGUICreatorHelpers::ActionLabel(option), selected))
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
	if (m_newWidgetAction != GameGUIActionType::NewGame)
	{
		m_newWidgetLaunchLevel.clear();
	}
	if (m_newWidgetAction != GameGUIActionType::SubPanel)
	{
		m_newWidgetTargetPanel.clear();
	}
}

void GameGUICreator::DrawCreateParentPanelField()
{
	GameGUIAsset& asset = CurrentGameGUI();
	const char* parentLabel = m_newWidgetParentPanel.empty() ? "<No Panel>" : m_newWidgetParentPanel.c_str();
	if (!ImGui::BeginCombo("Parent Panel", parentLabel))
	{
		return;
	}
	if (ImGui::Selectable("<No Panel>", m_newWidgetParentPanel.empty()))
	{
		m_newWidgetParentPanel.clear();
		m_newWidgetTargetPanel.clear();
	}
	for (const GameGUIWidgetDef& panel : asset.widgets)
	{
		if (panel.type != "Panel")
		{
			continue;
		}
		const bool selected = m_newWidgetParentPanel == panel.name;
		if (ImGui::Selectable(panel.name.c_str(), selected))
		{
			m_newWidgetParentPanel = panel.name;
			m_activeEditingPanel = panel.name;
			if (m_newWidgetTargetPanel == panel.name)
			{
				m_newWidgetTargetPanel.clear();
			}
		}
		if (selected)
		{
			ImGui::SetItemDefaultFocus();
		}
	}
	ImGui::EndCombo();
}

void GameGUICreator::DrawCreateTargetPanelField()
{
	if (m_newWidgetAction != GameGUIActionType::SubPanel)
	{
		return;
	}
	if (m_newWidgetParentPanel.empty())
	{
		m_newWidgetTargetPanel.clear();
		ImGui::TextDisabled("Choose a parent panel before selecting a target.");
		return;
	}

	GameGUIAsset& asset = CurrentGameGUI();
	const char* targetLabel = m_newWidgetTargetPanel.empty() ? "<Select Panel>" : m_newWidgetTargetPanel.c_str();
	if (ImGui::BeginCombo("Target Panel", targetLabel))
	{
		for (const GameGUIWidgetDef& panel : asset.widgets)
		{
			if (panel.type != "Panel" || panel.name == m_newWidgetParentPanel)
			{
				continue;
			}
			const bool selected = m_newWidgetTargetPanel == panel.name;
			if (ImGui::Selectable(panel.name.c_str(), selected))
			{
				m_newWidgetTargetPanel = panel.name;
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}

void GameGUICreator::DrawCreateLaunchLevelField()
{
	if (m_newWidgetAction != GameGUIActionType::NewGame)
	{
		m_newWidgetLaunchLevel.clear();
		return;
	}

	// The launch-level picker follows the same ImGui combo pattern as the action
	// combo above: compute the label, open the combo, draw each selectable row,
	// and preserve keyboard focus on the selected row.
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

void GameGUICreator::DrawCreateButtonPopup()
{
	DrawCreateWidgetPopupHeader("Create a button widget:");
	ImGui::Separator();
	ImGui::TextUnformatted("Binding");
	DrawCreateParentPanelField();
	DrawCreateActionField();
	DrawCreateLaunchLevelField();
	DrawCreateTargetPanelField();

	const bool invalidSubPanel = m_newWidgetAction == GameGUIActionType::SubPanel &&
		(m_newWidgetParentPanel.empty() || m_newWidgetTargetPanel.empty() || m_newWidgetParentPanel == m_newWidgetTargetPanel);
	ImGui::BeginDisabled(invalidSubPanel);
	if (ImGui::Button("Create"))
	{
		AddButtonWidget();
		SyncRuntimePreview();
		Root::Current().Debugger().LogMessage("Create Button requested");
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	if (invalidSubPanel)
	{
		ImGui::TextDisabled("Sub Panel requires different source and target panels.");
	}

	DrawCreateWidgetPopupFooter();
}

void GameGUICreator::DrawCreatePanelPopup()
{
	DrawCreateWidgetPopupHeader("Create a panel widget:");
	ImGui::Checkbox("Visible on load", &m_newPanelVisible);

	if (ImGui::Button("Create"))
	{
		AddPanelWidget();
		SyncRuntimePreview();
		Root::Current().Debugger().LogMessage("Create Panel requested");
		ImGui::CloseCurrentPopup();
	}

	DrawCreateWidgetPopupFooter();
}

void GameGUICreator::DrawCreateImagePopup()
{
	DrawCreateWidgetPopupHeader("Create an image widget:");
	DrawCreateParentPanelField();
	DrawCreateWidgetTextureField();

	if (ImGui::Button("Create"))
	{
		AddImageWidget();
		SyncRuntimePreview();
		Root::Current().Debugger().LogMessage("Create Image requested");
		ImGui::CloseCurrentPopup();
	}

	DrawCreateWidgetPopupFooter();
}

void GameGUICreator::DrawCreateProgressBarPopup()
{
	DrawCreateWidgetPopupHeader("Create a progress bar widget:");
	DrawCreateParentPanelField();
	DrawCreateWidgetTextureField();

	if (ImGui::Button("Create"))
	{
		AddProgressBarWidget();
		SyncRuntimePreview();
		Root::Current().Debugger().LogMessage("Create Progress Bar requested");
		ImGui::CloseCurrentPopup();
	}

	DrawCreateWidgetPopupFooter();
}

void GameGUICreator::DrawCreateTextPopup()
{
	DrawCreateWidgetPopupHeader("Create a text widget:");
	DrawCreateParentPanelField();

	if (ImGui::Button("Create"))
	{
		AddTextWidget();
		SyncRuntimePreview();
		Root::Current().Debugger().LogMessage("Create Text requested");
		ImGui::CloseCurrentPopup();
	}

	DrawCreateWidgetPopupFooter();
}


