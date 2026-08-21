#include "Engine/UI/GameGUIAssetUtils.h"

#include <cassert>

namespace
{
	GameGUIWidgetDef Panel(const char* name)
	{
		GameGUIWidgetDef panel;
		panel.type = "Panel";
		panel.name = name;
		return panel;
	}

	GameGUIWidgetDef SubPanelButton(const char* name, const char* parent, const char* target)
	{
		GameGUIWidgetDef button;
		button.type = "Button";
		button.name = name;
		button.parentName = parent;
		button.action = GameGUIActionType::SubPanel;
		button.targetPanel = target;
		return button;
	}
}

int main()
{
	GameGUIAsset asset;
	asset.widgets.push_back(Panel("Main"));
	asset.widgets.push_back(Panel("Options"));
	asset.widgets.push_back(SubPanelButton("Open Options", "Main", "Options"));
	asset.widgets.push_back(SubPanelButton("Back", "Options", "Main"));

	const GameGUIPanelTransition forward = GameGUIAssetUtils::ResolveSubPanelTransition(asset, "Open Options");
	assert(forward);
	assert(forward.sourcePanel->name == "Main");
	assert(forward.targetPanel->name == "Options");

	const GameGUIPanelTransition back = GameGUIAssetUtils::ResolveSubPanelTransition(asset, "Back");
	assert(back);
	assert(back.sourcePanel->name == "Options");
	assert(back.targetPanel->name == "Main");

	asset.widgets.push_back(SubPanelButton("Missing", "Main", "Credits"));
	assert(!GameGUIAssetUtils::ResolveSubPanelTransition(asset, "Missing"));

	asset.widgets.push_back(SubPanelButton("Self", "Main", "Main"));
	assert(!GameGUIAssetUtils::ResolveSubPanelTransition(asset, "Self"));

	asset.widgets.push_back(SubPanelButton("Standalone", "", "Options"));
	assert(!GameGUIAssetUtils::ResolveSubPanelTransition(asset, "Standalone"));
	return 0;
}
