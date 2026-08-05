#include "Engine/UI/GameGUICreatorHelpers.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/Component.h"
#include "Engine/Core/StbImage.h"

#include <imgui.h>
#include <MYGUI/MyGUI_Colour.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <vector>

namespace GameGUICreatorHelpers {
	float ReadFloatField(const std::string& value, float fallback)
	{
		if (value.empty()) return fallback;
		try { return std::stof(value); } catch (...) { return fallback; }
	}

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

	std::filesystem::path ResolveTexturePath(const std::string& texturePath)
	{
		if (texturePath.empty())
		{
			return {};
		}
		std::filesystem::path path(texturePath);
		if (path.is_absolute())
		{
			return path;
		}
		const std::filesystem::path sourceRoot = SourceRoot();
		const std::filesystem::path assetsRoot = sourceRoot / "assets";
		const std::string normalized = path.generic_string();
		const std::string sourceRootName = sourceRoot.filename().generic_string();
		if (!sourceRootName.empty() && normalized.rfind(sourceRootName, 0) == 0)
		{
			std::string remainder = normalized.substr(sourceRootName.size());
			while (!remainder.empty() && (remainder.front() == '/' || remainder.front() == '\\'))
			{
				remainder.erase(remainder.begin());
			}
			if (!remainder.empty())
			{
				return (sourceRoot / std::filesystem::path(remainder)).lexically_normal();
			}
		}
		if (normalized.rfind("assets/", 0) == 0 || normalized.rfind("assets\\", 0) == 0 || normalized == "assets")
		{
			return (sourceRoot / path).lexically_normal();
		}
		if (normalized.rfind("textures/", 0) == 0 || normalized.rfind("textures\\", 0) == 0 || normalized == "textures")
		{
			return (assetsRoot / path).lexically_normal();
		}
		return (sourceRoot / path).lexically_normal();
	}

	bool GetTextureDimensions(const std::filesystem::path& path, int& width, int& height)
	{
		try
		{
			StbImage image;
			image.loadFromFile(path.generic_string());
			width = std::max(1, image.getWidth());
			height = std::max(1, image.getHeight());
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool RefreshTextureBaseline(GameGUIWidgetDef& widget, const std::string& texturePath, bool useProgressTexture)
	{
		if (texturePath.empty())
		{
			return false;
		}

		const std::filesystem::path resolvedPath = ResolveTexturePath(texturePath);
		int width = 0;
		int height = 0;
		if (!GetTextureDimensions(resolvedPath, width, height))
		{
			return false;
		}

		widget.textureWidth = width;
		widget.textureHeight = height;
		widget.defaultWidth = width;
		widget.defaultHeight = height;
		if (!useProgressTexture)
		{
			widget.texture = texturePath;
		}
		return true;
	}

	std::string MakePortableTexturePath(const std::filesystem::path& absolutePath)
	{
		// Prefer storing a project-relative path when the texture lives under assets/.
		std::error_code ec;
		const std::filesystem::path assetsRoot = SourceRoot() / "assets";
		const std::filesystem::path relativeToAssets = Root::Current().FileSystemRef().Relative(absolutePath, assetsRoot, ec);

		// If the relative conversion succeeds, keep the shorter portable path in the asset file.
		if (!ec && !relativeToAssets.empty())
		{
			return relativeToAssets.generic_string();
		}

		// Otherwise fall back to the original absolute path so the selection is still usable.
		return absolutePath.generic_string();
	}

	namespace
	{
		std::vector<std::filesystem::path> EnumerateTextures()
		{
			std::vector<std::filesystem::path> textures;
			std::error_code ec;
			const std::filesystem::path root = TextureDirectory();

			// If the texture root is missing, return an empty list instead of surfacing a filesystem error.
			if (!std::filesystem::exists(root, ec) || ec)
			{
				return textures;
			}

			// Walk only the top-level texture folder so the dropdown shows a flat list.
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, ec))
			{
				// Stop early if the filesystem iterator reports an error.
				if (ec)
				{
					break;
				}

				// Only keep regular files with supported image extensions.
				if (entry.is_regular_file() && IsSupportedTextureFile(entry.path()))
				{
					textures.push_back(entry.path());
				}
			}

			// Alphabetical order keeps the combo stable and predictable for the user.
			std::sort(textures.begin(), textures.end());
			return textures;
		}
	}

	bool DrawTextureCombo(const char* label, std::string& texturePath, bool allowEmpty, const char* emptyLabel)
	{
		// The collapsed label shows the current value when the combo is closed.
		bool changed = false;
		const char* currentLabel = texturePath.empty() ? (allowEmpty ? emptyLabel : "<MyGUI Default>") : texturePath.c_str();
		if (ImGui::BeginCombo(label, currentLabel))
		{
			// When empty textures are allowed, expose an explicit "no texture" choice first.
			if (allowEmpty && ImGui::Selectable(emptyLabel, texturePath.empty()))
			{
				texturePath.clear();
				changed = true;
			}

			// EnumerateTextures() returns every supported texture under assets/textures.
			// Each row stores the portable path so the asset can be reloaded later.
			const std::vector<std::filesystem::path> textures = EnumerateTextures();

			for (const std::filesystem::path& tex : textures)
			{
				const std::string portable = MakePortableTexturePath(tex);

				// `selected` controls the highlight state for the currently active texture.
				const bool selected = texturePath == portable;

				if (ImGui::Selectable(portable.c_str(), selected))
				{
					// Clicking a texture updates the caller's string and reports that a change occurred.
					texturePath = portable;
					changed = true;
				}
			}
			// EndCombo closes the dropdown and finalizes the widget for this frame.
			ImGui::EndCombo();
		}
		return changed;
	}

	bool DrawProgressBindingControls(GameGUIWidgetDef& widget, Scene* scene)
	{
		bool changed = false;
		if (!scene)
		{
			ImGui::TextDisabled("No active Scene is available.");
			return false;
		}
		const char* entityLabel = widget.bindEntity.empty() ? "<Select Entity>" : widget.bindEntity.c_str();
		if (ImGui::BeginCombo("Entity", entityLabel))
		{
			for (const auto& entity : scene->Entities())
			{
				if (!entity) continue;
				const bool selected = widget.bindEntity == entity->Name();
				if (ImGui::Selectable(entity->Name().c_str(), selected))
				{
					widget.bindEntity = entity->Name();
					widget.bindComponent.clear();
					widget.bindMember.clear();
					widget.bindEvent.clear();
					changed = true;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		Entity* boundEntity = FindEntity(scene, widget.bindEntity);
		Component* boundComponent = boundEntity ? boundEntity->GetComponentByName(widget.bindComponent) : nullptr;
		ImGui::BeginDisabled(!boundEntity);
		const char* componentLabel = widget.bindComponent.empty() ? "<Select Component>" : widget.bindComponent.c_str();
		if (ImGui::BeginCombo("Component", componentLabel))
		{
			for (Component* component : boundEntity ? boundEntity->Components() : std::vector<Component*>{})
			{
				if (!component || component->GetBindableMembers().empty()) continue;
				const bool selected = widget.bindComponent == component->Name();
				if (ImGui::Selectable(component->Name(), selected))
				{
					widget.bindComponent = component->Name();
					widget.bindMember.clear();
					widget.bindEvent.clear();
					changed = true;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(!boundComponent);
		const char* memberLabel = widget.bindMember.empty() ? "<Select Value>" : widget.bindMember.c_str();
		std::vector<BindableMember> bindableMembers = boundComponent ? boundComponent->GetBindableMembers() : std::vector<BindableMember>{};
		if (ImGui::BeginCombo("Value", memberLabel))
		{
			for (const BindableMember& member : bindableMembers)
			{
				if (member.typeName != "int" && member.typeName != "float")
				{
					continue;
				}
				const bool selected = widget.bindMember == member.name;
				const char* label = member.displayName.empty() ? member.name.c_str() : member.displayName.c_str();
				if (ImGui::Selectable(label, selected))
				{
					widget.bindMember = member.name;
					changed = true;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		return changed;
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
			widget.panelButtonUseSkin = readField("\"panelButtonUseSkin\":", widgetPos).find("false") == std::string::npos;
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
			if (!widget.texture.empty() && RefreshTextureBaseline(widget, widget.texture, false))
			{
			}
			widget.layer = readField("\"layer\":", widgetPos);
			widget.x = ReadIntField(readField("\"x\":", widgetPos));
			widget.y = ReadIntField(readField("\"y\":", widgetPos));
			widget.width = ReadIntField(readField("\"width\":", widgetPos), widget.defaultWidth);
			widget.height = ReadIntField(readField("\"height\":", widgetPos), widget.defaultHeight);
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





