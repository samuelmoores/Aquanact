#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class GameGUIActionType
{
	None,
	NewGame,
};

struct GameGUIWidgetDef
{
	std::string type;
	std::string name;
	std::string parentName;
	std::string skin;
	std::string text;
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
	std::vector<GameGUIWidgetDef> widgets;
	bool savedOnDisk = false;
};

