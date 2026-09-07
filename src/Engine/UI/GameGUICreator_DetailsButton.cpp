#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace
{
	bool IsAudioFile(const std::filesystem::path& path)
	{
		const std::string extension = path.extension().string();
		return extension == ".wav" || extension == ".mp3" || extension == ".ogg" || extension == ".flac";
	}
}

namespace
{
	bool IsButtonControlledByPanel(const GameGUIAsset& asset, const GameGUIWidgetDef& widget)
	{
		return std::any_of(asset.widgets.begin(), asset.widgets.end(), [&widget](const GameGUIWidgetDef& candidate)
		{
			return candidate.type == "Panel" && candidate.name == widget.parentName;
		});
	}

	void RenameButtonWidget(GameGUIAsset& asset, GameGUIWidgetDef& widget, const char* newName)
	{
		if (!newName || !*newName || widget.name == newName)
		{
			return;
		}

		const std::string previousName = widget.name;
		widget.name = newName;
		widget.text = widget.name;
		for (GameGUIWidgetDef& otherWidget : asset.widgets)
		{
			if (otherWidget.parentName == previousName)
			{
				otherWidget.parentName = widget.name;
			}
		}
	}

	void DrawSceneCombo(const char* label, std::string& selectedScene)
	{
		SceneManager& sceneManager = Root::Current().Scenes();
		std::vector<std::string> sceneNames = sceneManager.SceneNames(SceneManager::SceneKind::Level);
		const std::vector<std::string> cutsceneNames = sceneManager.SceneNames(SceneManager::SceneKind::Cutscene);
		sceneNames.insert(sceneNames.end(), cutsceneNames.begin(), cutsceneNames.end());

		const char* sceneLabel = selectedScene.empty() ? "<Select Scene>" : selectedScene.c_str();
		if (ImGui::BeginCombo(label, sceneLabel))
		{
			for (const std::string& sceneName : sceneNames)
			{
				const bool selected = selectedScene == sceneName;
				if (ImGui::Selectable(sceneName.c_str(), selected))
				{
					selectedScene = sceneName;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		if (sceneNames.empty())
		{
			ImGui::TextDisabled("No levels or cutscenes are available.");
		}
	}
}

void GameGUICreator::DrawButtonLaunchSceneField(GameGUIWidgetDef& widget)
{
	if (widget.action != GameGUIActionType::NewGame)
	{
		widget.launchLevel.clear();
		return;
	}

	const std::string previousScene = widget.launchLevel;
	DrawSceneCombo("Level to load", widget.launchLevel);
	if (widget.launchLevel != previousScene)
	{
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
		ProjectManager& projects = Root::Current().Projects();
		if (!projects.CurrentProjectPath().empty())
		{
			projects.SaveProject(projects.CurrentProjectPath(), Root::Current().Scenes());
		}
	}
}

void GameGUICreator::DrawButtonWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	// Button widgets are the most interactive editor case, because name, caption,
	// skin state, size, and parent-panel membership all interact with one another.
	const bool controlledByPanel = IsButtonControlledByPanel(asset, widget);

	char buttonText[256] = {};
	std::snprintf(buttonText, sizeof(buttonText), "%s", widget.text.c_str());
	if (ImGui::InputText("Button text", buttonText, sizeof(buttonText)))
	{
		if (IsWidgetNameAvailable(asset, buttonText, &widget))
		{
			RenameButtonWidget(asset, widget, buttonText);
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}
	}

	if (ImGui::Checkbox("Use button skin", &widget.useSkin))
	{
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	const auto audioRoot = GameGUICreatorHelpers::SourceRoot() / "assets";
	std::string focusSoundLabel = widget.focusSound.empty() ? "<No focus sound>" : widget.focusSound;
	if (ImGui::BeginCombo("Focus sound", focusSoundLabel.c_str()))
	{
		if (ImGui::Selectable("<No focus sound>", widget.focusSound.empty()))
		{
			widget.focusSound.clear();
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}
		std::error_code ec;
		if (std::filesystem::exists(audioRoot, ec))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(audioRoot, ec))
			{
				if (ec || !entry.is_regular_file() || !IsAudioFile(entry.path())) continue;
				const auto relative = std::filesystem::relative(entry.path(), audioRoot, ec).generic_string();
				if (ImGui::Selectable(relative.c_str(), widget.focusSound == relative))
				{
					widget.focusSound = relative;
					SyncRuntimePreview();
					SaveSelectedRoleGUI();
				}
			}
		}
		ImGui::EndCombo();
	}

	GameGUIActionType action = widget.action;
	if (ImGui::BeginCombo("Action", GameGUICreatorHelpers::ActionLabel(action)))
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
				widget.action = action;
				if (action != GameGUIActionType::NewGame)
				{
					widget.launchLevel.clear();
				}
				if (action != GameGUIActionType::SubPanel)
				{
					widget.targetPanel.clear();
				}
				SyncRuntimePreview();
				SaveSelectedRoleGUI();
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	// Keep the New Game destination directly beneath the action selector so it
	// is immediately visible when editing the widget's behavior.
	DrawButtonLaunchSceneField(widget);

	if (!controlledByPanel)
	{
		// Standalone buttons keep an editable size. Panel-owned buttons are sized
		// by the panel layout code instead.
		if (m_dimensionRequestWidgetName != widget.name)
		{
			m_dimensionRequestWidgetName = widget.name;
			m_dimensionRequestWidth = widget.width;
			m_dimensionRequestHeight = widget.height;
		}

		// Treat the stored position as the image center in the editor.
		int buttonPosition[2] = { widget.x + widget.width / 2, widget.y + widget.height / 2 };
		if (ImGui::DragInt2("Position", buttonPosition, 1.0f))
		{
			widget.x = buttonPosition[0] - widget.width / 2;
			widget.y = buttonPosition[1] - widget.height / 2;
			SyncRuntimePreview();
		}


		float textColour[3] = { 0.0f, 0.0f, 0.0f };
		std::sscanf(widget.textColor.c_str(), "%f %f %f", &textColour[0], &textColour[1], &textColour[2]);
		if (ImGui::ColorEdit3("Text color", textColour))
		{
			char colourValue[96] = {};
			std::snprintf(colourValue, sizeof(colourValue), "%.3f %.3f %.3f", textColour[0], textColour[1], textColour[2]);
			widget.textColor = colourValue;
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}
	}

	// Parent panel selection keeps the hierarchy editable after a rename.
	DrawWidgetParentPanelField(asset, widget);

	if (widget.action == GameGUIActionType::SubPanel)
	{
		const std::string sourcePanel = OwningPanelName(asset, widget);
		if (sourcePanel.empty())
		{
			widget.targetPanel.clear();
			ImGui::TextDisabled("Sub Panel buttons must belong to a panel.");
		}
		else
		{
			const bool targetExists = std::any_of(asset.widgets.begin(), asset.widgets.end(), [&widget](const GameGUIWidgetDef& candidate)
			{
				return candidate.type == "Panel" && candidate.name == widget.targetPanel;
			});
			const bool targetValid = targetExists && widget.targetPanel != sourcePanel;
			const char* targetLabel = widget.targetPanel.empty() ? "<Select Panel>" : widget.targetPanel.c_str();
			if (ImGui::BeginCombo("Target panel", targetLabel))
			{
				for (const GameGUIWidgetDef& candidate : asset.widgets)
				{
					if (candidate.type != "Panel" || candidate.name == sourcePanel)
					{
						continue;
					}
					const bool selected = widget.targetPanel == candidate.name;
					if (ImGui::Selectable(candidate.name.c_str(), selected))
					{
						widget.targetPanel = candidate.name;
						SyncRuntimePreview();
						SaveSelectedRoleGUI();
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			if (!widget.targetPanel.empty() && !targetValid)
			{
				ImGui::TextDisabled("Target panel is missing or matches the source; choose a replacement.");
			}
		}
	}

}
