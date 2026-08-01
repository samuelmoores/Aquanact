#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/EngineGUI.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/UI/GameGUICreatorHelpers.h"

#include <MYGUI/MyGUI_Colour.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

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
		return true;
	}

	return false;
}

GameGUIAsset& GameGUICreator::CurrentRoleGUI()
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

std::filesystem::path GameGUICreator::GUIPathFor(const GameGUIAsset& asset) const
{
	return GameGUICreatorHelpers::AssetDirectory() / (asset.name + ".json");
}

std::size_t GameGUICreator::GUIIndex(GUIRole role)
{
	return static_cast<std::size_t>(role);
}

const char* GameGUICreator::GUIName(GUIRole role)
{
	return ::GUIName(GUIIndex(role));
}

void GameGUICreator::EndFrame() {}

void GameGUICreator::OpenTexturePicker(TexturePickerTarget target)
{
	m_texturePickerTarget = target;
	m_showTexturePickerPopup = true;
	m_texturePickerSelectedPath.clear();
	if (m_texturePickerRootDirectory.empty())
	{
		m_texturePickerRootDirectory = GameGUICreatorHelpers::SourceRoot() / "assets" / "textures";
	}
	m_texturePickerCurrentDirectory = m_texturePickerRootDirectory;
}

void GameGUICreator::AddButtonWidget()
{
	GameGUIAsset& asset = CurrentRoleGUI();
	GameGUIWidgetDef widget;
	widget.type = "Button";
	widget.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "Button";
	widget.text = widget.name;
	widget.texture = m_newWidgetTexture;
	widget.layer = "Main";
	widget.action = m_newWidgetAction;
	widget.launchLevel = m_newWidgetLaunchLevel;
	asset.widgets.push_back(widget);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::AddImageWidget()
{
	GameGUIAsset& asset = CurrentRoleGUI();
	GameGUIWidgetDef widget;
	widget.type = "Image";
	widget.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "Image";
	widget.texture = m_newWidgetTexture;
	widget.layer = "Main";
	widget.width = 100;
	widget.height = 100;
	asset.widgets.push_back(widget);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::AddProgressBarWidget()
{
	GameGUIAsset& asset = CurrentRoleGUI();
	GameGUIWidgetDef widget = m_pendingProgressBarWidget;
	if (widget.name.empty())
	{
		widget.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "ProgressBar";
	}
	widget.type = "ProgressBar";
	widget.layer = "Main";
	if (widget.texture.empty())
	{
		widget.texture = m_newWidgetTexture;
	}
	asset.widgets.push_back(widget);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::DeleteSelectedWidget()
{
	GameGUIAsset& asset = CurrentRoleGUI();
	if (m_selectedWidgetIndex < 0 || m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		return;
	}

	asset.widgets.erase(asset.widgets.begin() + static_cast<std::ptrdiff_t>(m_selectedWidgetIndex));
	if (asset.widgets.empty())
	{
		m_selectedWidgetIndex = -1;
	}
	else if (m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	}
	SaveSelectedRoleGUI();
	SyncRuntimePreview();
}




