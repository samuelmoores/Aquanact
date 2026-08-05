#include "Engine/UI/GameGUICreator.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

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
}

void GameGUICreator::DrawPanelWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	// Panels are layout containers, so this editor shows both the panel frame controls
	// and the child-button layout controls in one place.
	if (ImGui::Button("Add Button"))
	{
		// Reuse the create popup to add a child button under this panel.
		m_newButtonParentPanel = widget.name;
		m_showCreateWidgetPopup = true;
		m_newWidgetIsImage = false;
		m_newWidgetIsPanel = false;
		m_newWidgetAction = GameGUIActionType::None;
		m_newWidgetLaunchLevel.clear();
		m_newWidgetName[0] = '\0';
		m_newWidgetTexture[0] = '\0';
	}

	// Renaming a panel must preserve the parent relationship for any nested child widgets.
	char nameBuffer[256] = {};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", widget.name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		const std::string previousName = widget.name;
		widget.name = nameBuffer;
		for (GameGUIWidgetDef& child : asset.widgets)
		{
			if (child.parentName == previousName)
			{
				child.parentName = widget.name;
			}
		}
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Panel controls");

	// Changing the panel skin only affects the outer frame, not the child-button layout.
	bool layoutChanged = ImGui::Checkbox("Show panel skin", &widget.useSkin);
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

	// Panel position is edited independently so the frame can be moved without changing size.
	int panelPosition[2] = { widget.x, widget.y };
	if (ImGui::DragInt2("Position", panelPosition, 1.0f))
	{
		widget.x = panelPosition[0];
		widget.y = panelPosition[1];
		SyncRuntimePreview();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset##PanelPosition"))
	{
		widget.x = 0;
		widget.y = 0;
		SyncRuntimePreview();
	}

	// Lock Size is kept for editor parity even though the current controls still edit a square size directly.
	if (ImGui::Checkbox("Lock Size", &m_lockWidgetSize) && m_lockWidgetSize)
	{
		m_lockedWidgetSizeRatio = 1.0f;
	}

	// The panel body itself is still square-sized in this editor.
	int panelSize = widget.width;
	if (ImGui::DragInt("Size", &panelSize, 1.0f, 1, 4000))
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

	// Uniform spacing exposes extra child-layout controls only when enabled.
	if (widget.uniformButtonSpacing)
	{
		layoutChanged |= ImGui::Checkbox("Horizontal layout", &widget.horizontalButtonLayout);
		layoutChanged |= ImGui::SliderInt("Panel padding", &widget.panelPadding, 0, 100);
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Button controls");

	// These settings control the buttons the panel owns, not the panel frame itself.
	layoutChanged |= ImGui::Checkbox("Show button skins", &widget.panelButtonUseSkin);
	layoutChanged |= ImGui::Checkbox("Uniform button spacing", &widget.uniformButtonSpacing);

	// Keep the size edit box stable when the selected panel changes.
	if (m_dimensionRequestWidgetName != widget.name)
	{
		m_dimensionRequestWidgetName = widget.name;
		m_dimensionRequestWidth = widget.panelButtonWidth;
		m_dimensionRequestHeight = widget.panelButtonHeight;
	}

	int requestedPanelButtonSize[2] = { m_dimensionRequestWidth, m_dimensionRequestHeight };
	if (ImGui::DragInt2("Button dimensions", requestedPanelButtonSize, 1.0f, 1, 4000))
	{
		m_dimensionRequestWidth = requestedPanelButtonSize[0];
		m_dimensionRequestHeight = requestedPanelButtonSize[1];
		widget.panelButtonWidth = std::max(1, m_dimensionRequestWidth);
		widget.panelButtonHeight = std::max(1, m_dimensionRequestHeight);
		SyncPanelChildButtonSizes(asset, widget);
		ApplyPanelButtonLayout(widget);
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	if (ImGui::Button("Set button dimensions"))
	{
		// Preserve the old skin-ratio behavior so manual sizing still respects the skin aspect.
		constexpr float skinWidth = 32.0f;
		constexpr float skinHeight = 21.0f;
		const float scaleX = static_cast<float>(std::max(1, requestedPanelButtonSize[0])) / skinWidth;
		const float scaleY = static_cast<float>(std::max(1, requestedPanelButtonSize[1])) / skinHeight;
		widget.panelButtonScale = std::max(0.1f, std::min(scaleX, scaleY));
		widget.panelButtonWidth = std::max(1, static_cast<int>(std::lround(skinWidth * widget.panelButtonScale)));
		widget.panelButtonHeight = std::max(1, static_cast<int>(std::lround(skinHeight * widget.panelButtonScale)));
		layoutChanged = true;
	}

	// Button text color is stored as a string, so convert to and from float triples for editing.
	float panelTextColour[3] = { 0.0f, 0.0f, 0.0f };
	std::sscanf(widget.panelButtonTextColor.c_str(), "%f %f %f", &panelTextColour[0], &panelTextColour[1], &panelTextColour[2]);
	if (ImGui::ColorEdit3("Button text color", panelTextColour))
	{
		char colourValue[96] = {};
		std::snprintf(colourValue, sizeof(colourValue), "%.3f %.3f %.3f", panelTextColour[0], panelTextColour[1], panelTextColour[2]);
		widget.panelButtonTextColor = colourValue;
		layoutChanged = true;
	}

	// Font size affects only the child buttons, so it participates in the same layout refresh.
	int panelFontSize = widget.panelButtonFontSize;
	if (ImGui::DragInt("Button font size", &panelFontSize, 1.0f, 1, 256))
	{
		widget.panelButtonFontSize = std::max(1, panelFontSize);
		layoutChanged = true;
	}

	if (layoutChanged)
	{
		// Any frame/child-layout change needs a reflow and preview refresh.
		ApplyPanelButtonLayout(widget);
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
}
