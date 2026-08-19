#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"

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
		RenameButtonWidget(asset, widget, buttonText);
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
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
	const char* parentLabel = widget.parentName.empty() ? "<No Panel>" : widget.parentName.c_str();
	if (ImGui::BeginCombo("Parent panel", parentLabel))
	{
		if (ImGui::Selectable("<No Panel>", widget.parentName.empty()))
		{
			widget.parentName.clear();
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}

		for (GameGUIWidgetDef& candidate : asset.widgets)
		{
			if (candidate.type != "Panel")
			{
				continue;
			}

			const bool selected = widget.parentName == candidate.name;
			if (ImGui::Selectable(candidate.name.c_str(), selected))
			{
				widget.parentName = candidate.name;
				ApplyPanelButtonLayout(candidate);
				SyncRuntimePreview();
				SaveSelectedRoleGUI();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Delete"))
	{
		DeleteSelectedWidget();
		SyncRuntimePreview();
	}
}
