#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class GameGUIActionType
{
	None,
	QuitGame,
	PauseGame,
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
	int fontSize = 0;
	bool visible = true;
	float alpha = 1.0f;
	GameGUIActionType action = GameGUIActionType::None;
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
