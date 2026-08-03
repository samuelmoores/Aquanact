#include "Engine/UI/GameGUICreatorHelpers.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Scene.h"

#include <MYGUI/MyGUI_Colour.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace GameGUICreatorHelpers {
	int ReadIntField(const std::string& value, int fallback)
	{
		if (value.empty()) return fallback;
		try { return std::stoi(value); } catch (...) { return fallback; }
	}

	std::filesystem::path SourceRoot()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT);
#else
		return std::filesystem::current_path();
#endif
	}

	const char* GUIName(std::size_t index)
	{
		static constexpr const char* names[] = { "Main Menu", "HUD", "Pause Menu", "Player UI" };
		return index < std::size(names) ? names[index] : "Unknown";
	}

	const char* GUIAssetName(std::size_t index)
	{
		static constexpr const char* names[] = { "MainMenu", "HUD", "PauseMenu", "PlayerUI" };
		return index < std::size(names) ? names[index] : "Unknown";
	}

	const char* ActionLabel(GameGUIActionType action)
	{
		switch (action) { case GameGUIActionType::NewGame: return "New Game"; default: return "None"; }
	}

	std::string ActionToString(GameGUIActionType action)
	{
		switch (action) { case GameGUIActionType::NewGame: return "NewGame"; default: return "None"; }
	}

	std::filesystem::path AssetDirectory()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / "gameGUI";
#else
		return std::filesystem::current_path() / "assets" / "gameGUI";
#endif
	}

	std::filesystem::path TextureDirectory()
	{
		return SourceRoot() / "assets" / "textures";
	}

	bool ParseBoolField(const std::string& value)
	{
		return value.find("true") != std::string::npos;
	}

	MyGUI::Colour ParseColour(const std::string& value, const MyGUI::Colour& fallback)
	{
		try { return value.empty() ? fallback : MyGUI::Colour(value); } catch (...) { return fallback; }
	}

	GameGUIActionType StringToAction(const std::string& value)
	{
		return value == "NewGame" ? GameGUIActionType::NewGame : GameGUIActionType::None;
	}

	bool WouldCreateParentCycle(const GameGUIAsset& asset, const std::string& childName, const std::string& parentName)
	{
		if (childName.empty() || parentName.empty()) return false;
		if (childName == parentName) return true;
		std::string currentParent = parentName;
		while (!currentParent.empty())
		{
			if (currentParent == childName) return true;
			auto it = std::find_if(asset.widgets.begin(), asset.widgets.end(), [&currentParent](const GameGUIWidgetDef& widget){ return widget.name == currentParent; });
			if (it == asset.widgets.end()) return false;
			currentParent = it->parentName;
		}
		return false;
	}

	Entity* FindEntity(Scene* scene, const std::string& name)
	{
		if (!scene) return nullptr;
		for (const auto& entity : scene->Entities())
		{
			if (entity && entity->Name() == name) return entity.get();
		}
		return nullptr;
	}

	bool IsSupportedTextureFile(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
		return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga";
	}

	std::string MakePortableTexturePath(const std::filesystem::path& absolutePath)
	{
		std::error_code ec;
		const std::filesystem::path relativeToAssets = Root::Current().FileSystemRef().Relative(absolutePath, SourceRoot() / "assets", ec);
		return (!ec && !relativeToAssets.empty()) ? relativeToAssets.generic_string() : absolutePath.generic_string();
	}

	GameGUIAsset LoadAssetFile(const std::filesystem::path& assetPath)
	{
		GameGUIAsset asset;
		asset.name = assetPath.stem().string();
		asset.savedOnDisk = true;
		std::ifstream file(assetPath);
		if (!file.is_open()) { asset.savedOnDisk = false; return asset; }
		std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		const auto readAssetField = [&contents](const std::string& key) -> std::string
		{
			const std::size_t keyPos = contents.find("\"" + key + "\"");
			const std::size_t colon = keyPos == std::string::npos ? std::string::npos : contents.find(':', keyPos);
			const std::size_t start = colon == std::string::npos ? std::string::npos : contents.find_first_not_of(" \t", colon + 1);
			if (start == std::string::npos) return {};
			if (contents[start] == '"') { const std::size_t end = contents.find('"', start + 1); return end == std::string::npos ? std::string{} : contents.substr(start + 1, end - start - 1); }
			const std::size_t end = contents.find_first_of(",\r\n}", start);
			return contents.substr(start, end - start);
		};
		const std::size_t namePos = contents.find("\"name\":");
		if (namePos != std::string::npos)
		{
			const std::size_t firstQuote = contents.find('"', namePos + 7);
			const std::size_t secondQuote = firstQuote == std::string::npos ? std::string::npos : contents.find('"', firstQuote + 1);
			if (firstQuote != std::string::npos && secondQuote != std::string::npos)
			{
				asset.name = contents.substr(firstQuote + 1, secondQuote - firstQuote - 1);
			}
		}
		if (contents.find("\"navigationMode\": \"TextHighlight\"") != std::string::npos)
		{
			asset.navigationMode = GameGUIMenuNavigationMode::TextHighlight;
		}
		else if (contents.find("\"navigationMode\": \"Boxed\"") != std::string::npos)
		{
			asset.navigationMode = GameGUIMenuNavigationMode::Boxed;
		}
		const char* skins[] = { "WindowFrameSkin", "PanelSkin", "ButtonSkin", "ButtonEmptySkin", "TabPanelSkin", "ClientDefaultSkin" };
		for (const char* skin : skins) if (contents.find(std::string("\"boxSkin\": \"") + skin + "\"") != std::string::npos) { asset.boxSkin = skin; break; }
		const std::string pointerSkin = readAssetField("pointerSkin");
		if (pointerSkin == "NavigationArrowRight1" || pointerSkin == "NavigationArrowRight2" || pointerSkin == "NavigationArrowRight3" || pointerSkin == "NavigationArrowRight4") asset.pointerSkin = pointerSkin;
		asset.boxPadding = ReadIntField(readAssetField("boxPadding"), asset.boxPadding);
		asset.boxOffsetX = ReadIntField(readAssetField("boxOffsetX"), asset.boxOffsetX);
		asset.boxOffsetY = ReadIntField(readAssetField("boxOffsetY"), asset.boxOffsetY);
		asset.pointerWidth = ReadIntField(readAssetField("pointerWidth"), asset.pointerWidth);
		asset.pointerHeight = ReadIntField(readAssetField("pointerHeight"), asset.pointerHeight);
		asset.pointerGap = ReadIntField(readAssetField("pointerGap"), asset.pointerGap);
		try { asset.highlightR = std::stof(readAssetField("highlightR")); } catch (...) {}
		try { asset.highlightG = std::stof(readAssetField("highlightG")); } catch (...) {}
		try { asset.highlightB = std::stof(readAssetField("highlightB")); } catch (...) {}
		std::size_t widgetPos = contents.find("\"type\": \"");
		while (widgetPos != std::string::npos)
		{
			GameGUIWidgetDef widget;
			const auto readField = [&contents](const std::string& key, std::size_t start) -> std::string
			{
				const std::size_t keyPos = contents.find(key, start);
				if (keyPos == std::string::npos) return {};
				const std::size_t valueStart = contents.find_first_not_of(" \t", keyPos + key.size());
				if (valueStart == std::string::npos) return {};
				if (contents[valueStart] == '"')
				{
					const std::size_t valueEnd = contents.find('"', valueStart + 1);
					return valueEnd == std::string::npos ? std::string{} : contents.substr(valueStart + 1, valueEnd - valueStart - 1);
				}
				const std::size_t valueEnd = contents.find_first_of(",\n}", valueStart);
				return contents.substr(valueStart, valueEnd - valueStart);
			};
			widget.type = readField("\"type\":", widgetPos);
			widget.name = readField("\"name\":", widgetPos);
			widget.parentName = readField("\"parent\":", widgetPos);
			widget.skin = readField("\"skin\":", widgetPos);
			widget.useSkin = readField("\"useSkin\":", widgetPos).find("false") == std::string::npos;
			widget.uniformButtonSpacing = readField("\"uniformButtonSpacing\":", widgetPos).find("true") != std::string::npos;
			widget.horizontalButtonLayout = readField("\"horizontalButtonLayout\":", widgetPos).find("true") != std::string::npos;
			widget.panelPadding = ReadIntField(readField("\"panelPadding\":", widgetPos), 10);
			widget.panelButtonWidth = ReadIntField(readField("\"panelButtonWidth\":", widgetPos), 100);
			widget.panelButtonHeight = ReadIntField(readField("\"panelButtonHeight\":", widgetPos), 30);
			widget.panelButtonTextColor = readField("\"panelButtonTextColor\":", widgetPos);
			if (widget.panelButtonTextColor.empty()) widget.panelButtonTextColor = "0 0 0";
			widget.text = readField("\"text\":", widgetPos);
			widget.textColor = readField("\"textColor\":", widgetPos);
			if (widget.textColor.empty()) widget.textColor = "0 0 0";
			widget.texture = readField("\"texture\":", widgetPos);
			widget.layer = readField("\"layer\":", widgetPos);
			widget.x = ReadIntField(readField("\"x\":", widgetPos));
			widget.y = ReadIntField(readField("\"y\":", widgetPos));
			widget.width = ReadIntField(readField("\"width\":", widgetPos), 100);
			widget.height = ReadIntField(readField("\"height\":", widgetPos), 30);
			widget.defaultTextureWidth = widget.textureWidth;
			widget.defaultTextureHeight = widget.textureHeight;
			widget.fontSize = ReadIntField(readField("\"fontSize\":", widgetPos), 0);
			widget.visible = ParseBoolField(readField("\"visible\":", widgetPos));
			widget.alpha = 1.0f;
			try { widget.alpha = std::stof(readField("\"alpha\":", widgetPos)); } catch (...) {}
			widget.highlightColor = readField("\"highlightColor\":", widgetPos);
			widget.clickedColor = readField("\"clickedColor\":", widgetPos);
			widget.action = StringToAction(readField("\"action\":", widgetPos));
			widget.launchLevel = readField("\"launchLevel\":", widgetPos);
			widget.bindEntity = readField("\"bindEntity\":", widgetPos);
			widget.bindComponent = readField("\"bindComponent\":", widgetPos);
			widget.bindMember = readField("\"bindMember\":", widgetPos);
			widget.bindEvent = readField("\"bindEvent\":", widgetPos);
			asset.widgets.push_back(widget);
			widgetPos = contents.find("\"type\": \"", widgetPos + 1);
		}
		for (GameGUIWidgetDef& widget : asset.widgets)
		{
			if (widget.type != "Button" || widget.text.empty() || widget.name == widget.text) continue;
			const std::string previousName = widget.name;
			widget.name = widget.text;
			for (GameGUIWidgetDef& other : asset.widgets) if (other.parentName == previousName) other.parentName = widget.name;
		}
		return asset;
	}

	GameGUIAsset MakeEmptyAsset(const char* name)
	{
		GameGUIAsset asset;
		asset.name = name;
		asset.savedOnDisk = false;
		return asset;
	}
}





