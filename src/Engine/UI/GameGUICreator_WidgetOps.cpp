#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/UI/GameGUICreatorHelpers.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

void GameGUICreator::AddButtonWidget()
{
	GameGUIAsset& asset = CurrentGameGUI();
	GameGUIWidgetDef widget;
	widget.type = "Button";
	widget.name = MakeUniqueWidgetName(asset, m_newWidgetName[0] != '\0' ? m_newWidgetName : "Button");
	widget.text = widget.name;
	widget.texture = m_newWidgetTexture;
	widget.layer = "Main";
	widget.parentName = m_newWidgetParentPanel;
	widget.action = m_newWidgetAction;
	widget.launchLevel = m_newWidgetLaunchLevel;
	widget.targetPanel = m_newWidgetTargetPanel;
	asset.widgets.push_back(widget);
	if (!m_newWidgetParentPanel.empty())
	{
		auto panel = std::find_if(asset.widgets.begin(), asset.widgets.end(), [this](const GameGUIWidgetDef& candidate) { return candidate.type == "Panel" && candidate.name == m_newWidgetParentPanel; });
		if (panel != asset.widgets.end())
		{
			// Apply the panel's current text defaults only when the button is
			// created. Subsequent layout edits must preserve per-button styling.
			GameGUIWidgetDef& newButton = asset.widgets.back();
			newButton.textColor = panel->panelButtonTextColor;
			newButton.fontName = panel->panelButtonFontName;
			newButton.fontSize = panel->panelButtonFontSize;
			ApplyPanelButtonLayout(*panel);
		}
	}
	m_newWidgetParentPanel.clear();
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::AddImageWidget()
{
	GameGUIAsset& asset = CurrentGameGUI();
	GameGUIWidgetDef widget;
	widget.type = "Image";
	widget.name = MakeUniqueWidgetName(asset, m_newWidgetName[0] != '\0' ? m_newWidgetName : "Image");
	widget.parentName = m_newWidgetParentPanel;
	widget.texture = m_newWidgetTexture;
	widget.layer = "Main";
	// New image widgets should start at the texture's native size when a texture has been chosen.
	if (GameGUICreatorHelpers::RefreshTextureBaseline(widget, widget.texture, false))
	{
		widget.width = widget.defaultWidth;
		widget.height = widget.defaultHeight;
	}
	assert(widget.defaultWidth > 0 && widget.defaultHeight > 0);
	{
		CenterWidget(widget);
	}
	asset.widgets.push_back(widget);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::AddProgressBarWidget()
{
	GameGUIAsset& asset = CurrentGameGUI();
	GameGUIWidgetDef widget;
	widget.type = "ProgressBar";
	widget.name = MakeUniqueWidgetName(asset, m_newWidgetName[0] != '\0' ? m_newWidgetName : "ProgressBar");
	widget.parentName = m_newWidgetParentPanel;
	widget.texture = m_newWidgetTexture;
	widget.layer = "Main";
	if (!GameGUICreatorHelpers::RefreshTextureBaseline(widget, widget.texture, true))
	{
		widget.defaultWidth = 256;
		widget.defaultHeight = 32;
	}
	widget.width = widget.defaultWidth;
	widget.height = widget.defaultHeight;
	CenterWidget(widget);
	asset.widgets.push_back(widget);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::AddPanelWidget()
{
	GameGUIAsset& asset = CurrentGameGUI();
	GameGUIWidgetDef panel;
	panel.type = "Panel";
	panel.name = MakeUniqueWidgetName(asset, m_newWidgetName[0] != '\0' ? m_newWidgetName : "Panel");
	panel.skin = "PanelSkin";
	panel.layer = "Main";
	panel.width = 300;
	panel.height = 300;
	panel.visible = m_newPanelVisible;
	{
		int framebufferWidth = 0;
		int framebufferHeight = 0;
		Root::Current().WindowRef().GetFramebufferSize(framebufferWidth, framebufferHeight);
		panel.x = std::max(0, (framebufferWidth - panel.width) / 2);
		panel.y = std::max(0, (framebufferHeight - panel.height) / 2);
	}
	asset.widgets.push_back(panel);
	m_activeEditingPanel = panel.name;
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::AddTextWidget()
{
	GameGUIAsset& asset = CurrentGameGUI();
	GameGUIWidgetDef widget;
	widget.type = "Text";
	widget.name = MakeUniqueWidgetName(asset, m_newWidgetName[0] != '\0' ? m_newWidgetName : "Text");
	widget.parentName = m_newWidgetParentPanel;
	widget.text = widget.name;
	widget.width =         500;
	widget.height =        100;
	widget.defaultWidth = widget.width;
	widget.defaultHeight = widget.height;
	widget.fontSize = 30;

	widget.layer = "Main";

	CenterWidget(widget);

	asset.widgets.push_back(widget);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::CenterWidget(GameGUIWidgetDef& widget)
{
	if (!widget.parentName.empty())
	{
		const GameGUIAsset& asset = CurrentGameGUI();
		auto parent = std::find_if(asset.widgets.begin(), asset.widgets.end(), [&widget](const GameGUIWidgetDef& candidate)
		{
			return candidate.name == widget.parentName;
		});
		if (parent != asset.widgets.end())
		{
			widget.x = std::max(0, (parent->width - widget.width) / 2);
			widget.y = std::max(0, (parent->height - widget.height) / 2);
			return;
		}
	}

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	Root::Current().WindowRef().GetFramebufferSize(framebufferWidth, framebufferHeight);
	widget.x = std::max(0, (framebufferWidth - widget.width) / 2);
	widget.y = std::max(0, (framebufferHeight - widget.height) / 2);
}

bool GameGUICreator::IsWidgetNameAvailable(const GameGUIAsset& asset, const std::string& name, const GameGUIWidgetDef* ignoredWidget) const
{
	if (name.empty())
	{
		return false;
	}
	return std::none_of(asset.widgets.begin(), asset.widgets.end(), [&name, ignoredWidget](const GameGUIWidgetDef& candidate)
	{
		return &candidate != ignoredWidget && candidate.name == name;
	});
}

std::string GameGUICreator::MakeUniqueWidgetName(const GameGUIAsset& asset, const std::string& preferredName) const
{
	const std::string baseName = preferredName.empty() ? "Widget" : preferredName;
	if (IsWidgetNameAvailable(asset, baseName))
	{
		return baseName;
	}
	for (int suffix = 2; ; ++suffix)
	{
		const std::string candidate = baseName + " " + std::to_string(suffix);
		if (IsWidgetNameAvailable(asset, candidate))
		{
			return candidate;
		}
	}
}

std::string GameGUICreator::OwningPanelName(const GameGUIAsset& asset, const GameGUIWidgetDef& widget) const
{
	if (widget.type == "Panel")
	{
		return widget.name;
	}

	std::string parentName = widget.parentName;
	for (std::size_t depth = 0; !parentName.empty() && depth < asset.widgets.size(); ++depth)
	{
		auto parent = std::find_if(asset.widgets.begin(), asset.widgets.end(), [&parentName](const GameGUIWidgetDef& candidate)
		{
			return candidate.name == parentName;
		});
		if (parent == asset.widgets.end())
		{
			return {};
		}
		if (parent->type == "Panel")
		{
			return parent->name;
		}
		parentName = parent->parentName;
	}
	return {};
}

void GameGUICreator::RefreshActiveEditingPanel()
{
	GameGUIAsset& asset = CurrentGameGUI();
	const bool activePanelExists = std::any_of(asset.widgets.begin(), asset.widgets.end(), [this](const GameGUIWidgetDef& widget)
	{
		return widget.type == "Panel" && widget.name == m_activeEditingPanel;
	});
	if (activePanelExists)
	{
		return;
	}

	auto firstPanel = std::find_if(asset.widgets.begin(), asset.widgets.end(), [](const GameGUIWidgetDef& widget)
	{
		return widget.type == "Panel";
	});
	m_activeEditingPanel = firstPanel == asset.widgets.end() ? std::string{} : firstPanel->name;
}

void GameGUICreator::DrawWidgetParentPanelField(GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	const char* parentLabel = widget.parentName.empty() ? "<No Panel>" : widget.parentName.c_str();
	if (!ImGui::BeginCombo("Parent panel", parentLabel))
	{
		return;
	}

	auto selectParent = [this, &asset, &widget](const std::string& parentName)
	{
		const std::string previousParent = widget.parentName;
		widget.parentName = parentName;
		if (widget.action == GameGUIActionType::SubPanel && widget.targetPanel == parentName)
		{
			widget.targetPanel.clear();
		}
		for (GameGUIWidgetDef& panel : asset.widgets)
		{
			if (panel.type == "Panel" && (panel.name == previousParent || panel.name == parentName))
			{
				ApplyPanelButtonLayout(panel);
			}
		}
		if (!parentName.empty())
		{
			m_activeEditingPanel = parentName;
		}
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	};

	if (ImGui::Selectable("<No Panel>", widget.parentName.empty()))
	{
		selectParent({});
	}
	for (const GameGUIWidgetDef& candidate : asset.widgets)
	{
		if (candidate.type != "Panel")
		{
			continue;
		}
		const bool selected = widget.parentName == candidate.name;
		if (ImGui::Selectable(candidate.name.c_str(), selected))
		{
			selectParent(candidate.name);
		}
		if (selected)
		{
			ImGui::SetItemDefaultFocus();
		}
	}
	ImGui::EndCombo();
}

void GameGUICreator::ApplyPanelButtonLayout(GameGUIWidgetDef& panel)
{
	GameGUIAsset& asset = CurrentGameGUI();
	panel.panelButtonSkin = "MultiListButtonSkin";
	int nativeWidth = 29;
	int nativeHeight = 26;
	if (panel.panelButtonSkin == "CheckBoxSkin") { nativeWidth = 23; nativeHeight = 21; }
	else if (panel.panelButtonSkin == "RadioButtonSkin") { nativeWidth = 21; nativeHeight = 20; }
	else if (panel.panelButtonSkin == "ButtonCloseSkin") { nativeWidth = 18; nativeHeight = 17; }
	else if (panel.panelButtonSkin == "EditBoxSkin") { nativeWidth = 29; nativeHeight = 26; }
	else if (panel.panelButtonSkin == "MenuBarSkin") { nativeWidth = 27; nativeHeight = 26; }
	else if (panel.panelButtonSkin == "MenuItemSkin") { nativeWidth = 25; nativeHeight = 20; }
	else if (panel.panelButtonSkin == "ListBoxItemSkin") { nativeWidth = 43; nativeHeight = 10; }
	else if (panel.panelButtonSkin == "ClientDefaultSkin") { nativeWidth = 66; nativeHeight = 59; }
	else if (panel.panelButtonSkin == "ClientTileSkin") { nativeWidth = 34; nativeHeight = 18; }
	else if (panel.panelButtonSkin == "ScrollPanelHSkin") { nativeWidth = 19; nativeHeight = 15; }
	else if (panel.panelButtonSkin == "ScrollPanelVSkin") { nativeWidth = 15; nativeHeight = 19; }
	else if (panel.panelButtonSkin == "PanelSkin") { nativeWidth = 23; nativeHeight = 22; }
	else if (panel.panelButtonSkin == "CaptionEmptySkin" || panel.panelButtonSkin == "CaptionSkin" || panel.panelButtonSkin == "CaptionWithButtonSkin") { nativeWidth = 68; nativeHeight = 28; }
	else if (panel.panelButtonSkin == "WindowFrameSkin") { nativeWidth = 23; nativeHeight = 20; }
	else if (panel.panelButtonSkin == "TabHeaderButtonSkin") { nativeWidth = 52; nativeHeight = 23; }
	else if (panel.panelButtonSkin == "TabPanelSkin") { nativeWidth = 43; nativeHeight = 39; }
	else if (panel.panelButtonSkin == "MenuItemNormalSkin") { nativeWidth = 43; nativeHeight = 10; }
	else if (panel.panelButtonSkin == "MultiListButtonSkin") { nativeWidth = 32; nativeHeight = 21; }

	if (panel.panelButtonWidth <= 0 || panel.panelButtonHeight <= 0)
	{
		panel.panelButtonWidth = std::max(1, static_cast<int>(std::lround(nativeWidth * panel.panelButtonScale)));
		panel.panelButtonHeight = std::max(1, static_cast<int>(std::lround(nativeHeight * panel.panelButtonScale)));
	}

	std::vector<GameGUIWidgetDef*> buttons;

	for (GameGUIWidgetDef& widget : asset.widgets)
	{
		if (widget.type == "Button" && widget.parentName == panel.name) 
			buttons.push_back(&widget);
	}

	for (GameGUIWidgetDef* button : buttons)
	{
		button->width = std::max(1, panel.panelButtonWidth);
		button->height = std::max(1, panel.panelButtonHeight);
		// Text style is intentionally not copied here. This function is also
		// called for panel size, spacing, position, and order changes; those
		// layout edits must not reset per-button font settings.
	}

	if (!panel.uniformButtonSpacing || buttons.empty()) 
		return;

	const float spacingFactor = 1.0f - std::clamp(static_cast<float>(panel.panelPadding) / 100.0f, 0.0f, 1.0f);

	if (panel.horizontalButtonLayout)
	{
		int totalWidth = 0;
		for (const GameGUIWidgetDef* button : buttons) totalWidth += button->width;
		const int availableSpace = std::max(0, panel.width - totalWidth);
		const int gap = panel.panelPadding >= 100
			? -6
			: buttons.size() > 1 ? static_cast<int>(std::lround((availableSpace / static_cast<float>(buttons.size() - 1)) * spacingFactor)) : 0;
		const int groupWidth = totalWidth + gap * static_cast<int>(buttons.size() - 1);
		int x = std::max(0, (panel.width - groupWidth) / 2);
		for (GameGUIWidgetDef* button : buttons) { button->x = x; button->y = (panel.height - button->height) / 2; x += button->width + gap; }
	}
	else
	{
		int totalHeight = 0;
		for (const GameGUIWidgetDef* button : buttons) 
			totalHeight += button->height;

		const int availableSpace = std::max(0, panel.height - totalHeight);

		const int gap = panel.panelPadding >= 100
			? -6
			: buttons.size() > 1 ? static_cast<int>(std::lround((availableSpace / static_cast<float>(buttons.size() - 1)) * spacingFactor)) : 0;

		const int groupHeight = totalHeight + gap * static_cast<int>(buttons.size() - 1);

		int y = std::max(0, (panel.height - groupHeight) / 2);

		for (GameGUIWidgetDef* button : buttons) 
		{ 
			button->x = (panel.width - button->width) / 2; 
			button->y = y; y += button->height + gap; 
		}
	}
}

void GameGUICreator::DeleteSelectedWidget()
{
	GameGUIAsset& asset = CurrentGameGUI();

	if (m_selectedWidgetIndex < 0 || m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		return;
	}

	const std::size_t selectedIndex = static_cast<std::size_t>(m_selectedWidgetIndex);
	const std::string deletedName = asset.widgets[selectedIndex].name;

	for (GameGUIWidgetDef& widget : asset.widgets)
	{
		if (widget.parentName == deletedName)
		{
			widget.parentName.clear();
		}
		if (widget.targetPanel == deletedName)
		{
			widget.targetPanel.clear();
		}
	}

	asset.widgets.erase(asset.widgets.begin() + static_cast<std::ptrdiff_t>(m_selectedWidgetIndex));

	if (asset.widgets.empty())
	{
		m_selectedWidgetIndex = -1;
	}
	else if (m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	}
	RefreshActiveEditingPanel();

	SaveSelectedRoleGUI();
	SyncRuntimePreview();
}
