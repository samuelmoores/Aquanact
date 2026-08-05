#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Camera.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Window.h"
#include "Engine/UI/EngineGUI.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/UI/GameGUI.h"

#include <MYGUI/MyGUI_ImageBox.h>
#include <MYGUI/MyGUI_Widget.h>
#include <imgui.h>

#include <MYGUI/MyGUI_Colour.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <functional>

namespace {
	const char* GUIName(std::size_t index)
	{
		static constexpr const char* names[] = { "Main Menu", "HUD" };
		return index < std::size(names) ? names[index] : "Unknown";
	}

	const char* GUIAssetName(std::size_t index)
	{
		static constexpr const char* names[] = { "MainMenu", "HUD", "PauseMenu", "PlayerUI" };
		return index < std::size(names) ? names[index] : "Unknown";
	}

	bool WouldCreateParentCycle(const GameGUIAsset& asset, const std::string& childName, const std::string& parentName)
	{
		if (childName.empty() || parentName.empty() || childName == parentName)
		{
			return childName == parentName;
		}

		std::string currentParent = parentName;
		while (!currentParent.empty())
		{
			if (currentParent == childName)
			{
				return true;
			}
			auto it = std::find_if(asset.widgets.begin(), asset.widgets.end(), [&currentParent](const GameGUIWidgetDef& widget) { return widget.name == currentParent; });
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
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga";
	}

	std::string MakePortableTexturePath(const std::filesystem::path& absolutePath)
	{
		std::error_code ec;
		const std::filesystem::path relativeToAssets = Root::Current().FileSystemRef().Relative(absolutePath, GameGUICreatorHelpers::SourceRoot() / "assets", ec);
		return (!ec && !relativeToAssets.empty()) ? relativeToAssets.generic_string() : absolutePath.generic_string();
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

// Lifecycle and frame flow

void GameGUICreator::startUp(Window& window)
{
	if (m_initialized) return;
	m_window = &window;
	m_assets.clear();
	m_assets.resize(GUIIndex(GUIRole::Count));
	for (std::size_t i = 0; i < m_assets.size(); ++i)
	{
		const std::filesystem::path assetPath = GameGUICreatorHelpers::AssetDirectory() / (std::string(GUIAssetName(i)) + ".json");
		m_assets[i] = std::filesystem::exists(assetPath) ? GameGUICreatorHelpers::LoadAssetFile(assetPath) : MakeEmptyAsset(GUIAssetName(i));
	}
	m_selectedGUI = GUIRole::MainMenu;
	m_selectedWidgetIndex = m_assets[GUIIndex(m_selectedGUI)].widgets.empty() ? -1 : 0;
	LoadNavigationSettingsFromAsset();
	m_initialized = true;
}

void GameGUICreator::shutDown()
{
	m_window = nullptr;
	m_initialized = false;
	m_assets.clear();
	m_selectedGUI = GUIRole::MainMenu;
	m_selectedWidgetIndex = -1;
}

void GameGUICreator::BeginFrame() {}

void GameGUICreator::EndFrame() {}

// External state and selection control

void GameGUICreator::CaptureEditorViewState(bool showAxis, bool showGrid)
{
	m_previousShowAxis = showAxis;
	m_previousShowGrid = showGrid;
	m_previousViewStateCaptured = true;
}

bool GameGUICreator::IsMainMenuSelected() const
{
	return m_selectedGUI == GUIRole::MainMenu;
}

std::string GameGUICreator::SelectedGUIAssetName() const
{
	return GUIAssetName(GUIIndex(m_selectedGUI));
}

bool GameGUICreator::SelectGUIAsset(const std::string& assetName)
{
	for (std::size_t i = 0; i < m_assets.size(); ++i)
	{
		if (m_assets[i].name != assetName && GUIAssetName(i) != assetName)
		{
			continue;
		}

		m_selectedGUI = static_cast<GUIRole>(i);
		m_selectedWidgetIndex = m_assets[i].widgets.empty() ? -1 : 0;
		LoadNavigationSettingsFromAsset();
		return true;
	}

	return false;
}

void GameGUICreator::SetMenuNavigationMode(MenuNavigationMode mode)
{
	m_menuNavigationMode = mode;
	if (!m_assets.empty()) CurrentGameGUI().navigationMode = mode;
	if (m_initialized)
	{
		SyncRuntimePreview();
	}
}

// Asset access and persistence helpers.
std::filesystem::path GameGUICreator::GUIPathFor(const GameGUIAsset& asset) const
{
	return GameGUICreatorHelpers::AssetDirectory() / (asset.name + ".json");
}

GameGUIAsset& GameGUICreator::CurrentGameGUI()
{
	return m_assets[GUIIndex(m_selectedGUI)];
}

const GameGUIAsset& GameGUICreator::CurrentRoleGUI() const
{
	return m_assets[GUIIndex(m_selectedGUI)];
}

GameGUIAsset& GameGUICreator::GUIFor(GUIRole role)
{
	return m_assets[GUIIndex(role)];
}

const GameGUIAsset& GameGUICreator::GUIFor(GUIRole role) const
{
	return m_assets[GUIIndex(role)];
}

// Simple role-to-index / role-to-label helpers.
std::size_t GameGUICreator::GUIIndex(GUIRole role)
{
	return static_cast<std::size_t>(role);
}

const char* GameGUICreator::GUIName(GUIRole role)
{
	return ::GUIName(GUIIndex(role));
}

void GameGUICreator::LoadNavigationSettingsFromAsset()
{
	if (m_assets.empty()) return;
	const GameGUIAsset& asset = CurrentGameGUI();
	m_menuNavigationMode = asset.navigationMode;
	m_boxPadding = asset.boxPadding;
	m_boxOffsetX = asset.boxOffsetX;
	m_boxOffsetY = asset.boxOffsetY;
	m_pointerWidth = asset.pointerWidth;
	m_pointerHeight = asset.pointerHeight;
	m_pointerGap = asset.pointerGap;
	m_highlightR = asset.highlightR;
	m_highlightG = asset.highlightG;
	m_highlightB = asset.highlightB;
	m_selectedR = asset.selectedR; m_selectedG = asset.selectedG; m_selectedB = asset.selectedB;
	const std::string boxSkins[] = { "WindowFrameSkin", "PanelSkin", "ButtonSkin", "ButtonEmptySkin", "TabPanelSkin", "ClientDefaultSkin" };
	m_boxSkinIndex = 0;
	for (int i = 0; i < 6; ++i) if (asset.boxSkin == boxSkins[i]) { m_boxSkinIndex = i; break; }
	const std::string pointerSkins[] = { "NavigationArrowRight1", "NavigationArrowRight2", "NavigationArrowRight3", "NavigationArrowRight4" };
	m_pointerSkinIndex = 0;
	for (int i = 0; i < 4; ++i) if (asset.pointerSkin == pointerSkins[i]) { m_pointerSkinIndex = i; break; }
}




