#pragma once

#include "Engine/UI/GameGUIAsset.h"
#include <MYGUI/MyGUI_Colour.h>
#include <filesystem>
#include <string>
class Level; class Entity;
namespace GameGUICreatorHelpers {
	int ReadIntField(const std::string& value, int fallback = 0);
	std::filesystem::path SourceRoot();
	const char* GUIName(std::size_t index);
	const char* GUIAssetName(std::size_t index);
	const char* ActionLabel(GameGUIActionType action);
	std::string ActionToString(GameGUIActionType action);
	bool ParseBoolField(const std::string& value);
	MyGUI::Colour ParseColour(const std::string& value, const MyGUI::Colour& fallback);
	GameGUIActionType StringToAction(const std::string& value);
	bool WouldCreateParentCycle(const GameGUIAsset& asset, const std::string& childName, const std::string& parentName);
	Entity* FindEntity(Level* level, const std::string& name);
	std::filesystem::path AssetDirectory();
	std::filesystem::path TextureDirectory();
	bool IsSupportedTextureFile(const std::filesystem::path& path);
	std::string MakePortableTexturePath(const std::filesystem::path& absolutePath);
	GameGUIAsset LoadAssetFile(const std::filesystem::path& assetPath);
	GameGUIAsset MakeEmptyAsset(const char* name);
}

