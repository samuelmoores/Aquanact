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
		if (value.empty())
		{
			return fallback;
		}

		try
		{
			return std::stof(value);
		}
		catch (...)
		{
			return fallback;
		}
	}

	int ReadIntField(const std::string& value, int fallback)
	{
		if (value.empty())
		{
			return fallback;
		}

		try
		{
			return std::stoi(value);
		}
		catch (...)
		{
			return fallback;
		}
	}

	std::filesystem::path SourceRoot()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT);
#else
		return std::filesystem::current_path();
#endif
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

	const char* GUIName(std::size_t index)
	{
		// These are the user-facing labels shown in the creator's GUI selector.
		static constexpr const char* names[] = { "Main Menu", "HUD", "Pause Menu", "Player UI" };
		return index < std::size(names) ? names[index] : "Unknown";
	}

	const char* GUIAssetName(std::size_t index)
	{
		// These are the file stems used when loading and saving the assets.
		static constexpr const char* names[] = { "MainMenu", "HUD", "PauseMenu", "PlayerUI" };
		return index < std::size(names) ? names[index] : "Unknown";
	}

	const char* ActionLabel(GameGUIActionType action)
	{
		switch (action)
		{
		case GameGUIActionType::NewGame:
			return "New Game";
		case GameGUIActionType::Pause:
			return "Pause";
		case GameGUIActionType::Resume:
			return "Resume";
		default:
			return "None";
		}
	}

	std::string ActionToString(GameGUIActionType action)
	{
		switch (action)
		{
		case GameGUIActionType::NewGame:
			return "NewGame";
		case GameGUIActionType::Pause:
			return "Pause";
		case GameGUIActionType::Resume:
			return "Resume";
		default:
			return "None";
		}
	}

	bool ParseBoolField(const std::string& value)
	{
		// The asset format stores booleans as textual JSON fragments.
		return value.find("true") != std::string::npos;
	}

	MyGUI::Colour ParseColour(const std::string& value, const MyGUI::Colour& fallback)
	{
		if (value.empty())
		{
			return fallback;
		}

		try
		{
			return MyGUI::Colour(value);
		}
		catch (...)
		{
			return fallback;
		}
	}

	GameGUIActionType StringToAction(const std::string& value)
	{
		if (value == "NewGame")
		{
			return GameGUIActionType::NewGame;
		}
		if (value == "Pause")
		{
			return GameGUIActionType::Pause;
		}
		if (value == "Resume")
		{
			return GameGUIActionType::Resume;
		}
		return GameGUIActionType::None;
	}

	bool WouldCreateParentCycle(const GameGUIAsset& asset, const std::string& childName, const std::string& parentName)
	{
		// Empty names cannot form a cycle.
		if (childName.empty() || parentName.empty())
		{
			return false;
		}

		// A widget cannot parent itself.
		if (childName == parentName)
		{
			return true;
		}

		// Walk upward through the parent chain and stop if we encounter the child.
		std::string currentParent = parentName;
		while (!currentParent.empty())
		{
			if (currentParent == childName)
			{
				return true;
			}

			const auto it = std::find_if(asset.widgets.begin(), asset.widgets.end(), [&currentParent](const GameGUIWidgetDef& widget)
			{
				return widget.name == currentParent;
			});
			if (it == asset.widgets.end())
			{
				return false;
			}

			currentParent = it->parentName;
		}

		return false;
	}

	Entity* FindEntity(Scene* scene, const std::string& name)
	{
		if (!scene)
		{
			return nullptr;
		}

		for (const auto& entity : scene->Entities())
		{
			if (entity && entity->Name() == name)
			{
				return entity.get();
			}
		}

		return nullptr;
	}

	bool IsSupportedTextureFile(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c)
		{
			return static_cast<char>(std::tolower(c));
		});

		return extension == ".png" ||
			extension == ".jpg" ||
			extension == ".jpeg" ||
			extension == ".bmp" ||
			extension == ".tga";
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

		// Older asset files may already include the project folder name.
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

		// Paths rooted at assets/ are resolved from the project root.
		if (normalized.rfind("assets/", 0) == 0 || normalized.rfind("assets\\", 0) == 0 || normalized == "assets")
		{
			return (sourceRoot / path).lexically_normal();
		}

		// Bare textures/ paths are interpreted relative to assets/.
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
			// Use the shared image loader so the editor follows the same decode path as runtime.
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

		// Keep the widget sizing fields aligned with the selected texture dimensions.
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
		// Prefer a project-relative path when possible so assets stay portable.
		std::error_code ec;
		const std::filesystem::path assetsRoot = SourceRoot() / "assets";
		const std::filesystem::path relativeToAssets = Root::Current().FileSystemRef().Relative(absolutePath, assetsRoot, ec);

		if (!ec && !relativeToAssets.empty())
		{
			return relativeToAssets.generic_string();
		}

		// Fall back to the absolute path when the file sits outside the project tree.
		return absolutePath.generic_string();
	}

	namespace
	{
		std::vector<std::filesystem::path> EnumerateTextures()
		{
			std::vector<std::filesystem::path> textures;
			std::error_code ec;
			const std::filesystem::path root = TextureDirectory();

			// If the texture root is missing, return an empty list instead of failing.
			if (!std::filesystem::exists(root, ec) || ec)
			{
				return textures;
			}

			// The combo stays flat by only listing the top-level texture directory.
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, ec))
			{
				if (ec)
				{
					break;
				}

				if (entry.is_regular_file() && IsSupportedTextureFile(entry.path()))
				{
					textures.push_back(entry.path());
				}
			}

			std::sort(textures.begin(), textures.end());
			return textures;
		}

		std::string ReadJsonStringField(const std::string& contents, const std::string& key, std::size_t start)
		{
			const std::size_t keyPos = contents.find(key, start);
			if (keyPos == std::string::npos)
			{
				return {};
			}

			std::size_t valueStart = contents.find_first_not_of(" \t", keyPos + key.size());
			if (valueStart == std::string::npos)
			{
				return {};
			}

			if (contents[valueStart] == '"')
			{
				const std::size_t valueEnd = contents.find('"', valueStart + 1);
				return valueEnd == std::string::npos ? std::string{} : contents.substr(valueStart + 1, valueEnd - valueStart - 1);
			}

			const std::size_t valueEnd = contents.find_first_of(",\n}", valueStart);
			return contents.substr(valueStart, valueEnd - valueStart);
		}
	}

	bool DrawTextureCombo(const char* label, std::string& texturePath, bool allowEmpty, const char* emptyLabel)
	{
		bool changed = false;
		const char* currentLabel = texturePath.empty() ? (allowEmpty ? emptyLabel : "<MyGUI Default>") : texturePath.c_str();

		if (ImGui::BeginCombo(label, currentLabel))
		{
			// Give the user an explicit "no texture" choice when the caller allows it.
			if (allowEmpty && ImGui::Selectable(emptyLabel, texturePath.empty()))
			{
				texturePath.clear();
				changed = true;
			}

			// Build the texture list once per open combo.
			const std::vector<std::filesystem::path> textures = EnumerateTextures();
			for (const std::filesystem::path& tex : textures)
			{
				const std::string portable = MakePortableTexturePath(tex);
				const bool selected = texturePath == portable;

				if (ImGui::Selectable(portable.c_str(), selected))
				{
					texturePath = portable;
					changed = true;
				}
			}

			ImGui::EndCombo();
		}

		return changed;
	}

	bool DrawProgressBindingControls(GameGUIWidgetDef& widget, Scene* scene)
	{
		bool changed = false;

		// Check if scene is invalid
		if (!scene)
		{
			ImGui::TextDisabled("No active Scene is available.");
			return false;
		}

		// Entity selection is the root of the binding chain.
		const char* entityLabel = widget.bindEntity.empty() ? "<Select Entity>" : widget.bindEntity.c_str();

		// ******** Entity select combo box *********
		// set widgets bound entity
		if (ImGui::BeginCombo("Entity", entityLabel))
		{
			for (const auto& entity : scene->Entities())
			{
				if (!entity)
				{
					continue;
				}

				const bool selected = widget.bindEntity == entity->Name();

				if (ImGui::Selectable(entity->Name().c_str(), selected))
				{
					// Switching the entity invalidates any component or member selection
					// that was attached to the previous entity.
					widget.bindEntity = entity->Name();
					widget.bindComponent.clear();
					widget.bindMember.clear();
					widget.bindEvent.clear();
					changed = true;
				}

				// Keep the currently selected entity visible when the combo opens.
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		// Resolve the current entity and component after any entity change above.
		Entity* boundEntity = FindEntity(scene, widget.bindEntity);
		Component* boundComponent = boundEntity ? boundEntity->GetComponentByName(widget.bindComponent) : nullptr;

		// The component picker is only meaningful once an entity is chosen.
		ImGui::BeginDisabled(!boundEntity);

		const char* componentLabel = widget.bindComponent.empty() ? "<Select Component>" : widget.bindComponent.c_str();

		// ******** Entity's component select combo box *********
		// Show every component here so the user can see what is attached to the
		// entity. The value dropdown below will explain whether that component
		// actually exposes any numeric bindable data.
		if (ImGui::BeginCombo("Component", componentLabel))
		{
			for (Component* component : boundEntity ? boundEntity->Components() : std::vector<Component*>{})
			{
				if (!component)
				{
					continue;
				}

				const bool selected = widget.bindComponent == component->Name();
				if (ImGui::Selectable(component->Name(), selected))
				{
					// Changing component invalidates the selected member and event.
					widget.bindComponent = component->Name();
					widget.bindMember.clear();
					widget.bindEvent.clear();
					changed = true;
				}

				// Keep the chosen component highlighted when the combo opens.
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();

		// The value picker only unlocks when a concrete component is selected.
		ImGui::BeginDisabled(!boundComponent);
		const char* memberLabel = widget.bindMember.empty() ? "<Select Value>" : widget.bindMember.c_str();
		std::vector<BindableMember> bindableMembers = boundComponent ? boundComponent->GetBindableMembers() : std::vector<BindableMember>{};
		bool hasNumericBindableValue = false;
		for (const BindableMember& member : bindableMembers)
		{
			if (member.typeName == "int" || member.typeName == "float")
			{
				hasNumericBindableValue = true;
				break;
			}
		}

		// ******** Components's value select combo box *********
		// If the component has no numeric bindable values, show a disabled
		// placeholder instead of leaving the user wondering why the list is empty.
		if (ImGui::BeginCombo("Value", memberLabel))
		{
			if (!hasNumericBindableValue)
			{
				ImGui::BeginDisabled(true);
				ImGui::Selectable("No numeric bindable values available", false);
				ImGui::EndDisabled();
			}
			else
			{
				for (const BindableMember& member : bindableMembers)
				{
					// Progress bars only bind to numeric members.
					if (member.typeName != "int" && member.typeName != "float")
					{
						continue;
					}

					const bool selected = widget.bindMember == member.name;
					const char* label = member.displayName.empty() ? member.name.c_str() : member.displayName.c_str();
					if (ImGui::Selectable(label, selected))
					{
						// Selecting the member completes the binding chain.
						widget.bindMember = member.name;
						changed = true;
					}

					// Keep the active member visible when the list opens.
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
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
		if (!file.is_open())
		{
			asset.savedOnDisk = false;
			return asset;
		}

		std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		// Small helper for the hand-rolled JSON-ish asset format.
		const auto readField = [&contents](const std::string& key, std::size_t start) -> std::string
		{
			return ReadJsonStringField(contents, key, start);
		};

		// Keep the explicit asset name if it exists in the file.
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

		// Navigation mode is stored as a text label in the asset file.
		if (contents.find("\"navigationMode\": \"TextHighlight\"") != std::string::npos)
		{
			asset.navigationMode = GameGUIMenuNavigationMode::TextHighlight;
		}
		else if (contents.find("\"navigationMode\": \"Boxed\"") != std::string::npos)
		{
			asset.navigationMode = GameGUIMenuNavigationMode::Boxed;
		}

		// Pull the fixed top-level asset settings first.
		const char* skins[] = { "WindowFrameSkin", "PanelSkin", "ButtonSkin", "ButtonEmptySkin", "TabPanelSkin", "ClientDefaultSkin" };
		for (const char* skin : skins)
		{
			if (contents.find(std::string("\"boxSkin\": \"") + skin + "\"") != std::string::npos)
			{
				asset.boxSkin = skin;
				break;
			}
		}

		const std::string pointerSkin = readField("pointerSkin", 0);
		if (pointerSkin == "NavigationArrowRight1" ||
			pointerSkin == "NavigationArrowRight2" ||
			pointerSkin == "NavigationArrowRight3" ||
			pointerSkin == "NavigationArrowRight4")
		{
			asset.pointerSkin = pointerSkin;
		}

		asset.boxPadding = ReadIntField(readField("boxPadding", 0), asset.boxPadding);
		asset.boxOffsetX = ReadIntField(readField("boxOffsetX", 0), asset.boxOffsetX);
		asset.boxOffsetY = ReadIntField(readField("boxOffsetY", 0), asset.boxOffsetY);
		asset.pointerWidth = ReadIntField(readField("pointerWidth", 0), asset.pointerWidth);
		asset.pointerHeight = ReadIntField(readField("pointerHeight", 0), asset.pointerHeight);
		asset.pointerGap = ReadIntField(readField("pointerGap", 0), asset.pointerGap);

		try { asset.highlightR = std::stof(readField("highlightR", 0)); } catch (...) {}
		try { asset.highlightG = std::stof(readField("highlightG", 0)); } catch (...) {}
		try { asset.highlightB = std::stof(readField("highlightB", 0)); } catch (...) {}

		// Each widget block is parsed independently so partially edited files still load.
		std::size_t widgetPos = contents.find("\"type\": \"");
		while (widgetPos != std::string::npos)
		{
			GameGUIWidgetDef widget;
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
			if (widget.panelButtonTextColor.empty())
			{
				widget.panelButtonTextColor = "0 0 0";
			}

			widget.text = readField("\"text\":", widgetPos);
			widget.textColor = readField("\"textColor\":", widgetPos);
			if (widget.textColor.empty())
			{
				widget.textColor = "0 0 0";
			}

			widget.texture = readField("\"texture\":", widgetPos);
			if (!widget.texture.empty())
			{
				RefreshTextureBaseline(widget, widget.texture, false);
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
			widget.focusSound = readField("\"focusSound\":", widgetPos);
			widget.action = StringToAction(readField("\"action\":", widgetPos));
			widget.launchLevel = readField("\"launchLevel\":", widgetPos);
			widget.bindEntity = readField("\"bindEntity\":", widgetPos);
			widget.bindComponent = readField("\"bindComponent\":", widgetPos);
			widget.bindMember = readField("\"bindMember\":", widgetPos);
			widget.bindEvent = readField("\"bindEvent\":", widgetPos);
			widget.panelButtonFocusSound = readField("\"panelButtonFocusSound\":", widgetPos);

			asset.widgets.push_back(widget);
			widgetPos = contents.find("\"type\": \"", widgetPos + 1);
		}

		// Older assets sometimes stored a button caption in `name`, so normalize that.
		for (GameGUIWidgetDef& widget : asset.widgets)
		{
			if (widget.type != "Button" || widget.text.empty() || widget.name == widget.text)
			{
				continue;
			}

			const std::string previousName = widget.name;
			widget.name = widget.text;
			for (GameGUIWidgetDef& other : asset.widgets)
			{
				if (other.parentName == previousName)
				{
					other.parentName = widget.name;
				}
			}
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
