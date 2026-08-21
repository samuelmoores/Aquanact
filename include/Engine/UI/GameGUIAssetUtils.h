#pragma once

#include "Engine/UI/GameGUIAsset.h"

#include <string>

struct GameGUIPanelTransition
{
	const GameGUIWidgetDef* button = nullptr;
	const GameGUIWidgetDef* sourcePanel = nullptr;
	const GameGUIWidgetDef* targetPanel = nullptr;
	std::string error;

	explicit operator bool() const
	{
		return button && sourcePanel && targetPanel && error.empty();
	}
};

namespace GameGUIAssetUtils
{
	const GameGUIWidgetDef* FindWidget(const GameGUIAsset& asset, const std::string& name);
	const GameGUIWidgetDef* FindOwningPanel(const GameGUIAsset& asset, const GameGUIWidgetDef& widget);
	GameGUIPanelTransition ResolveSubPanelTransition(const GameGUIAsset& asset, const std::string& buttonName);
}
