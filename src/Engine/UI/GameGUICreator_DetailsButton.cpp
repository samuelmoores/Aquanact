#include "Engine/UI/GameGUICreator.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

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

		int requestedButtonSize[2] = { m_dimensionRequestWidth, m_dimensionRequestHeight };
		if (ImGui::DragInt2("Button dimensions", requestedButtonSize, 1.0f, 1, 4000))
		{
			m_dimensionRequestWidth = requestedButtonSize[0];
			m_dimensionRequestHeight = requestedButtonSize[1];
			widget.width = std::max(1, m_dimensionRequestWidth);
			widget.height = std::max(1, m_dimensionRequestHeight);
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}

		if (ImGui::Button("Set dimensions"))
		{
			constexpr float skinWidth = 32.0f;
			constexpr float skinHeight = 21.0f;
			const float scaleX = static_cast<float>(std::max(1, requestedButtonSize[0])) / skinWidth;
			const float scaleY = static_cast<float>(std::max(1, requestedButtonSize[1])) / skinHeight;
			const float scale = std::max(0.1f, std::min(scaleX, scaleY));
			widget.width = std::max(1, static_cast<int>(std::lround(skinWidth * scale)));
			widget.height = std::max(1, static_cast<int>(std::lround(skinHeight * scale)));
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
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
}
