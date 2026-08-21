#include "Engine/UI/GameGUIAssetUtils.h"

namespace GameGUIAssetUtils
{
	const GameGUIWidgetDef* FindWidget(const GameGUIAsset& asset, const std::string& name)
	{
		for (const GameGUIWidgetDef& widget : asset.widgets)
		{
			if (widget.name == name)
			{
				return &widget;
			}
		}
		return nullptr;
	}

	const GameGUIWidgetDef* FindOwningPanel(const GameGUIAsset& asset, const GameGUIWidgetDef& widget)
	{
		if (widget.type == "Panel")
		{
			return &widget;
		}

		std::string parentName = widget.parentName;
		for (std::size_t depth = 0; !parentName.empty() && depth < asset.widgets.size(); ++depth)
		{
			const GameGUIWidgetDef* parent = FindWidget(asset, parentName);
			if (!parent)
			{
				return nullptr;
			}
			if (parent->type == "Panel")
			{
				return parent;
			}
			parentName = parent->parentName;
		}
		return nullptr;
	}

	GameGUIPanelTransition ResolveSubPanelTransition(const GameGUIAsset& asset, const std::string& buttonName)
	{
		GameGUIPanelTransition result;
		result.button = FindWidget(asset, buttonName);
		if (!result.button || result.button->type != "Button")
		{
			result.error = "button is missing or is not a Button";
			return result;
		}
		if (result.button->action != GameGUIActionType::SubPanel)
		{
			result.error = "button action is not SubPanel";
			return result;
		}

		result.sourcePanel = FindOwningPanel(asset, *result.button);
		if (!result.sourcePanel)
		{
			result.error = "button has no owning Panel";
			return result;
		}

		result.targetPanel = FindWidget(asset, result.button->targetPanel);
		if (!result.targetPanel || result.targetPanel->type != "Panel")
		{
			result.error = "target Panel is missing";
			return result;
		}
		if (result.sourcePanel == result.targetPanel)
		{
			result.error = "source and target Panels match";
			return result;
		}
		return result;
	}
}
