#pragma once

#include "Engine/UI/GameGUIAsset.h"
#include <MYGUI/MyGUI_Colour.h>
#include <filesystem>
#include <functional>
#include <string>
class Scene; class Entity;
struct GameGUIWidgetDef;
class Component;
namespace GameGUICreatorHelpers {
	// Numeric field parsing
	int ReadIntField(const std::string& value, int fallback = 0);
	float ReadFloatField(const std::string& value, float fallback = 0.0f);

	// Project and asset paths
	std::filesystem::path SourceRoot();
	std::filesystem::path AssetDirectory();
	std::filesystem::path TextureDirectory();

	// GUI labels and enum conversion
	const char* GUIName(std::size_t index);
	const char* GUIAssetName(std::size_t index);
	const char* ActionLabel(GameGUIActionType action);
	std::string ActionToString(GameGUIActionType action);
	bool ParseBoolField(const std::string& value);
	MyGUI::Colour ParseColour(const std::string& value, const MyGUI::Colour& fallback);
	GameGUIActionType StringToAction(const std::string& value);

	// Asset relationships and lookup
	bool WouldCreateParentCycle(const GameGUIAsset& asset, const std::string& childName, const std::string& parentName);
	Entity* FindEntity(Scene* scene, const std::string& name);

	// Texture helpers
	bool IsSupportedTextureFile(const std::filesystem::path& path);
	std::filesystem::path ResolveTexturePath(const std::string& texturePath);
	bool GetTextureDimensions(const std::filesystem::path& path, int& width, int& height);
	bool RefreshTextureBaseline(GameGUIWidgetDef& widget, const std::string& texturePath, bool useProgressTexture);
	std::string MakePortableTexturePath(const std::filesystem::path& absolutePath);
	bool DrawTextureCombo(const char* label, std::string& texturePath, bool allowEmpty, const char* emptyLabel);

	// Progress-bar binding editor
	bool DrawProgressBindingControls(GameGUIWidgetDef& widget, Scene* scene);

	// Asset loading
	GameGUIAsset LoadAssetFile(const std::filesystem::path& assetPath);
	GameGUIAsset MakeEmptyAsset(const char* name);
}


