#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class GameGUIActionType
{
	None,
	NewGame,
	Pause,
	Resume,
};

enum class GameGUIMenuNavigationMode
{
	Pointer,
	TextHighlight,
	Boxed,
};

struct GameGUIWidgetDef
{
	// Identity and hierarchy
	// These fields identify the widget and define how it is nested under other widgets.
	std::string type;
	std::string name;
	std::string parentName;

	// Base skinning
	// These fields control the widget's primary skin and whether skin-specific styling is enabled.
	std::string skin;
	bool useSkin = true;

	// Text and imagery
	// Widgets can display text, a texture, or both depending on their type.
	std::string text;
	std::string textColor = "0 0 0";
	std::string texture;

	// Placement and geometry
	// These values describe where the widget lives and how large it is.
	std::string layer = "Main";
	int x = 0;
	int y = 0;
	int defaultWidth = 0;
	int defaultHeight = 0;
	int width = 0;
	int height = 0;
	int textureWidth = 0;
	int textureHeight = 0;

	// Generic visibility and opacity
	// These apply to every widget regardless of type.
	int fontSize = 0;
	std::string fontName;
	bool visible = true;
	float alpha = 1.0f;

	// Specialized panel-button layout
	// These fields only matter for panel widgets and their child buttons.
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
	std::string panelButtonFocusSound;

	// Button interaction and progress-bar styling
	// These fields are used by interactive widgets and progress bars.
	std::string highlightColor;
	std::string clickedColor;
	std::string focusSound;
	GameGUIActionType action = GameGUIActionType::None;
	std::string launchLevel;

	// Runtime binding
	// These fields connect a widget to a live entity/component/member at runtime.
	std::string bindEntity;
	std::string bindComponent;
	std::string bindMember;
	std::string bindEvent;
};

struct GameGUIAsset
{
	// Asset identity and creator settings
	std::string name = "UntitledGameGUI";
	GameGUIMenuNavigationMode navigationMode = GameGUIMenuNavigationMode::Pointer;

	// Main frame visuals
	std::string boxSkin = "WindowFrameSkin";
	std::string pointerSkin = "NavigationArrowRight1";
	int boxPadding = 8;
	int boxOffsetX = 0;
	int boxOffsetY = 0;
	int pointerWidth = 40;
	int pointerHeight = 40;
	int pointerGap = 24;

	// Editor highlight colors
	float highlightR = 1.0f;
	float highlightG = 1.0f;
	float highlightB = 0.0f;
	float selectedR = 1.0f;
	float selectedG = 1.0f;
	float selectedB = 1.0f;

	// Widget collection and persistence state
	std::vector<GameGUIWidgetDef> widgets;
	bool savedOnDisk = false;
};

