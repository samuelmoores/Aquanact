#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace
{
	void SyncPanelChildButtonSizes(GameGUIAsset& asset, const GameGUIWidgetDef& panel)
	{
		// Panel children inherit their size from the panel's shared button-dimension settings.
		for (GameGUIWidgetDef& child : asset.widgets)
		{
			if (child.type == "Button" && child.parentName == panel.name)
			{
				child.width = panel.panelButtonWidth;
				child.height = panel.panelButtonHeight;
			}
		}
	}

	void SyncPanelChildButtonTextStyle(GameGUIAsset& asset, const GameGUIWidgetDef& panel)
	{
		for (GameGUIWidgetDef& child : asset.widgets)
		{
			if (child.type == "Button" && child.parentName == panel.name)
			{
				child.textColor = panel.panelButtonTextColor;
				child.fontName = panel.panelButtonFontName;
				child.fontSize = panel.panelButtonFontSize;
			}
		}
	}

	bool DrawPanelButtonFocusSound(GameGUIWidgetDef& widget)
	{
		bool changed = false;
		const auto audioRoot = GameGUICreatorHelpers::SourceRoot() / "assets";
		const std::string panelSoundFileName = widget.panelButtonFocusSound.empty()
			? std::string("<No panel focus sound>")
			: std::filesystem::path(widget.panelButtonFocusSound).filename().string();
		if (!ImGui::BeginCombo("Button focus sound", panelSoundFileName.c_str()))
		{
			return false;
		}

		if (ImGui::Selectable("<No panel focus sound>", widget.panelButtonFocusSound.empty()))
		{
			widget.panelButtonFocusSound.clear();
			changed = true;
		}
		std::error_code ec;
		if (std::filesystem::exists(audioRoot, ec))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(audioRoot, ec))
			{
				if (ec || !entry.is_regular_file())
				{
					continue;
				}
				const std::string extension = entry.path().extension().string();
				if (extension != ".wav" && extension != ".mp3" && extension != ".ogg" && extension != ".flac")
				{
					continue;
				}

				const std::string relative = std::filesystem::relative(entry.path(), audioRoot, ec).generic_string();
				const std::string fileName = entry.path().filename().string();
				if (ImGui::Selectable(fileName.c_str(), widget.panelButtonFocusSound == relative))
				{
					widget.panelButtonFocusSound = relative;
					changed = true;
				}
			}
		}
		ImGui::EndCombo();
		return changed;
	}
}

void GameGUICreator::DrawPanelWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	// Panels are layout containers, so this editor shows both the panel frame controls
	// and the child-button layout controls in one place.
	if (ImGui::Button("Add Button"))
	{
		// Reuse the create popup to add a child button under this panel.
		OpenCreateWidgetPopup(NewWidgetType::Button);
		m_newWidgetParentPanel = widget.name;
	}

	// Renaming a panel must preserve the parent relationship for any nested child widgets.
	char nameBuffer[256] = {};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", widget.name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		if (!IsWidgetNameAvailable(asset, nameBuffer, &widget))
		{
			ImGui::TextDisabled("Panel names must be non-empty and unique.");
		}
		else
		{
		const std::string previousName = widget.name;
		widget.name = nameBuffer;
		for (GameGUIWidgetDef& child : asset.widgets)
		{
			if (child.parentName == previousName)
			{
				child.parentName = widget.name;
			}
			if (child.targetPanel == previousName)
			{
				child.targetPanel = widget.name;
			}
		}
		if (m_activeEditingPanel == previousName)
		{
			m_activeEditingPanel = widget.name;
		}
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
		}
	}

	if (ImGui::Checkbox("Visible on load", &widget.visible))
	{
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
	bool layoutChanged = ImGui::Checkbox("Horizontal layout", &widget.horizontalButtonLayout);

	ImGui::SeparatorText("Panel controls");

	// Position is stored in screen coordinates for root panels.
	int panelPositionX = widget.x;
	int panelPositionY = widget.y;
	ImGui::TextUnformatted("Position");
	ImGui::SetNextItemWidth(100.0f);
	const bool panelPositionXChanged = ImGui::DragInt("X##PanelPosition", &panelPositionX, 1.0f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	const bool panelPositionYChanged = ImGui::DragInt("Y##PanelPosition", &panelPositionY, 1.0f);
	ImGui::SameLine();
	const bool resetPanelPosition = ImGui::SmallButton("Reset##PanelPosition");
	if (panelPositionXChanged || panelPositionYChanged || resetPanelPosition)
	{
		widget.x = resetPanelPosition ? 0 : panelPositionX;
		widget.y = resetPanelPosition ? 0 : panelPositionY;
		SyncRuntimePreview();
	}

	// The panel body itself is square-sized in this editor.
	int panelSize = widget.width;
	ImGui::TextUnformatted("Size");
	ImGui::SetNextItemWidth(210.0f);
	if (ImGui::DragInt("##PanelSize", &panelSize, 1.0f, 1, 4000))
	{
		widget.width = std::max(1, panelSize);
		widget.height = widget.width;
		ApplyPanelButtonLayout(widget);
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset##PanelSize"))
	{
		widget.width = 300;
		widget.height = 300;
		ApplyPanelButtonLayout(widget);
		SyncRuntimePreview();
	}

	// Changing the panel skin only affects the outer frame, not the child-button layout.
	layoutChanged |= ImGui::Checkbox("Show panel skin", &widget.useSkin);
	widget.panelButtonSkin = "MultiListButtonSkin";
	const char* panelSkins[] = { "PanelSkin", "WindowFrameSkin", "TabPanelSkin", "ClientDefaultSkin" };
	int skinIndex = 0;
	for (int i = 0; i < IM_ARRAYSIZE(panelSkins); ++i)
	{
		if (widget.skin == panelSkins[i])
		{
			skinIndex = i;
			break;
		}
	}
	if (widget.useSkin && ImGui::Combo("Panel skin", &skinIndex, panelSkins, IM_ARRAYSIZE(panelSkins)))
	{
		widget.skin = panelSkins[skinIndex];
		layoutChanged = true;
	}

	ImGui::SeparatorText("Button controls");

	// Keep the size edit box stable when the selected panel changes.
	if (m_dimensionRequestWidgetName != widget.name)
	{
		m_dimensionRequestWidgetName = widget.name;
		m_dimensionRequestWidth = widget.panelButtonWidth;
		m_dimensionRequestHeight = widget.panelButtonHeight;
	}

	int requestedPanelButtonWidth = m_dimensionRequestWidth;
	int requestedPanelButtonHeight = m_dimensionRequestHeight;
	ImGui::TextUnformatted("Size");
	ImGui::SetNextItemWidth(100.0f);
	const bool buttonWidthChanged = ImGui::DragInt("X##PanelButtonSize", &requestedPanelButtonWidth, 1.0f, 1, 4000);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	const bool buttonHeightChanged = ImGui::DragInt("Y##PanelButtonSize", &requestedPanelButtonHeight, 1.0f, 1, 4000);
	ImGui::SameLine();
	const bool resetButtonSize = ImGui::SmallButton("Reset##PanelButtonSize");
	if (buttonWidthChanged || buttonHeightChanged || resetButtonSize)
	{
		m_dimensionRequestWidth = resetButtonSize ? 100 : requestedPanelButtonWidth;
		m_dimensionRequestHeight = resetButtonSize ? 30 : requestedPanelButtonHeight;
		widget.panelButtonWidth = std::max(1, m_dimensionRequestWidth);
		widget.panelButtonHeight = std::max(1, m_dimensionRequestHeight);
		SyncPanelChildButtonSizes(asset, widget);
		ApplyPanelButtonLayout(widget);
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	// These settings control the buttons the panel owns, not the panel frame itself.
	layoutChanged |= ImGui::Checkbox("Show button skins", &widget.panelButtonUseSkin);
	layoutChanged |= ImGui::Checkbox("Uniform button spacing", &widget.uniformButtonSpacing);
	if (widget.uniformButtonSpacing)
	{
		layoutChanged |= ImGui::SliderInt("Spacing", &widget.panelPadding, 0, 100);
	}
	if (DrawPanelButtonFocusSound(widget))
	{
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	// Font size and color are shared by all child buttons.
	ImGui::SeparatorText("Button text");
	int panelFontSize = widget.panelButtonFontSize;
	if (ImGui::DragInt("Button font size", &panelFontSize, 1.0f, 1, 256))
	{
		widget.panelButtonFontSize = std::max(1, panelFontSize);
		SyncPanelChildButtonTextStyle(asset, widget);
		layoutChanged = true;
	}

	float panelTextColour[3] = { 0.0f, 0.0f, 0.0f };
	std::sscanf(widget.panelButtonTextColor.c_str(), "%f %f %f", &panelTextColour[0], &panelTextColour[1], &panelTextColour[2]);
	if (ImGui::ColorEdit3("Button text color", panelTextColour))
	{
		char colourValue[96] = {};
		std::snprintf(colourValue, sizeof(colourValue), "%.3f %.3f %.3f", panelTextColour[0], panelTextColour[1], panelTextColour[2]);
		widget.panelButtonTextColor = colourValue;
		SyncPanelChildButtonTextStyle(asset, widget);
		layoutChanged = true;
	}

	std::vector<std::size_t> buttonIndices;
	for (std::size_t index = 0; index < asset.widgets.size(); ++index)
	{
		const GameGUIWidgetDef& candidate = asset.widgets[index];
		if (candidate.type == "Button" && candidate.parentName == widget.name)
		{
			buttonIndices.push_back(index);
		}
	}

	ImGui::SeparatorText("Button order");
	if (buttonIndices.empty())
	{
		ImGui::TextDisabled("No buttons belong to this panel.");
	}
	else
	{
		for (std::size_t orderIndex = 0; orderIndex < buttonIndices.size(); ++orderIndex)
		{
			const std::size_t widgetIndex = buttonIndices[orderIndex];
			ImGui::PushID(static_cast<int>(widgetIndex));

			ImGui::BeginDisabled(orderIndex == 0);
			const bool moveUp = ImGui::ArrowButton("##MoveButtonUp", ImGuiDir_Up);
			ImGui::EndDisabled();
			ImGui::SameLine();

			ImGui::BeginDisabled(orderIndex + 1 >= buttonIndices.size());
			const bool moveDown = ImGui::ArrowButton("##MoveButtonDown", ImGuiDir_Down);
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::Text("%zu. %s", orderIndex + 1, asset.widgets[widgetIndex].name.c_str());
			ImGui::PopID();

			if (moveUp)
			{
				const std::size_t adjacentIndex = buttonIndices[orderIndex - 1];
				std::swap(asset.widgets[widgetIndex], asset.widgets[adjacentIndex]);
				if (!widget.uniformButtonSpacing)
				{
					std::swap(asset.widgets[widgetIndex].x, asset.widgets[adjacentIndex].x);
					std::swap(asset.widgets[widgetIndex].y, asset.widgets[adjacentIndex].y);
				}
				layoutChanged = true;
				break;
			}
			if (moveDown)
			{
				const std::size_t adjacentIndex = buttonIndices[orderIndex + 1];
				std::swap(asset.widgets[widgetIndex], asset.widgets[adjacentIndex]);
				if (!widget.uniformButtonSpacing)
				{
					std::swap(asset.widgets[widgetIndex].x, asset.widgets[adjacentIndex].x);
					std::swap(asset.widgets[widgetIndex].y, asset.widgets[adjacentIndex].y);
				}
				layoutChanged = true;
				break;
			}
		}
	}

	if (layoutChanged)
	{
		// Any frame/child-layout change needs a reflow and preview refresh.
		ApplyPanelButtonLayout(widget);
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
}
