#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct UIWidgetDef
{
	std::string type;
	std::string name;
	std::string skin;
	std::string text;
	std::string layer = "Main";
	int x = 0;
	int y = 0;
	int width = 100;
	int height = 30;
	bool visible = true;
	float alpha = 1.0f;
};

struct UIAsset
{
	std::string name = "UntitledUI";
	std::vector<UIWidgetDef> widgets;
	bool savedOnDisk = false;
};
