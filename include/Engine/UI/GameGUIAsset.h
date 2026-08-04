#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class GameGUIActionType
{
	None,
	NewGame,
};

enum class GameGUIMenuNavigationMode
{
	Pointer,
	TextHighlight,
	Boxed,
};

struct GameGUIWidgetDef
{
	std::string type;
	std::string name;
	std::string parentName;
	std::string skin;
	bool useSkin = true;
	bool uniformButtonSpacing = false;
	bool panelButtonUseSkin = true;
	std::string panelButtonSkin = "MultiListButtonSkin";
	float panelButtonScale = 1.0f;
	bool horizontalButtonLayout = false;
	int panelPadding = 10;
	int panelButtonWidth = 100;
	int panelButtonHeight = 30;
	std::string panelButtonTextColor = "0 0 0";
	std::string panelButtonFontName;
	int panelButtonFontSize = 10;
	std::string text;
	std::string textColor = "0 0 0";
	std::string texture;
	std::string layer = "Main";
	int x = 0;
	int y = 0;
	int width = 100;
	int height = 30;
	int textureWidth = 100;
	int textureHeight = 30;
	int defaultTextureWidth = 100;
	int defaultTextureHeight = 30;
	int fontSize = 0;
	std::string fontName;
	bool visible = true;
	float alpha = 1.0f;
	std::string highlightColor;
	std::string clickedColor;
	GameGUIActionType action = GameGUIActionType::None;
	std::string launchLevel;
	std::string bindEntity;
	std::string bindComponent;
	std::string bindMember;
	std::string bindEvent;
};

struct GameGUIAsset
{
	std::string name = "UntitledGameGUI";
	GameGUIMenuNavigationMode navigationMode = GameGUIMenuNavigationMode::Pointer;
	std::string boxSkin = "WindowFrameSkin";
	std::string pointerSkin = "NavigationArrowRight1";
	int boxPadding = 8;
	int boxOffsetX = 0;
	int boxOffsetY = 0;
	int pointerWidth = 40;
	int pointerHeight = 40;
	int pointerGap = 24;
	float highlightR = 1.0f;
	float highlightG = 1.0f;
	float highlightB = 0.0f;
	float selectedR = 1.0f;
	float selectedG = 1.0f;
	float selectedB = 1.0f;
	std::vector<GameGUIWidgetDef> widgets;
	bool savedOnDisk = false;
};

