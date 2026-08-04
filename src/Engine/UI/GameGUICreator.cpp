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

void GameGUICreator::SetMenuNavigationMode(MenuNavigationMode mode)
{
	m_menuNavigationMode = mode;
	if (!m_assets.empty()) CurrentRoleGUI().navigationMode = mode;
	if (m_initialized)
	{
		SyncRuntimePreview();
	}
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

void GameGUICreator::LoadNavigationSettingsFromAsset()
{
	if (m_assets.empty()) return;
	const GameGUIAsset& asset = CurrentRoleGUI();
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
	widget.parentName = m_newButtonParentPanel;
	widget.action = m_newWidgetAction;
	widget.launchLevel = m_newWidgetLaunchLevel;
	asset.widgets.push_back(widget);
	if (!m_newButtonParentPanel.empty())
	{
		auto panel = std::find_if(asset.widgets.begin(), asset.widgets.end(), [this](const GameGUIWidgetDef& candidate) { return candidate.type == "Panel" && candidate.name == m_newButtonParentPanel; });
		if (panel != asset.widgets.end()) ApplyPanelButtonLayout(*panel);
	}
	m_newButtonParentPanel.clear();
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

void GameGUICreator::AddPanelWidget()
{
	GameGUIAsset& asset = CurrentRoleGUI();
	GameGUIWidgetDef panel;
	panel.type = "Panel";
	panel.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "Panel";
	panel.skin = "PanelSkin";
	panel.layer = "Main";
	panel.width = 300;
	panel.height = 300;
	asset.widgets.push_back(panel);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	SaveSelectedRoleGUI();
}

void GameGUICreator::ApplyPanelButtonLayout(GameGUIWidgetDef& panel)
{
	GameGUIAsset& asset = CurrentRoleGUI();
	panel.panelButtonSkin = "MultiListButtonSkin";
	int nativeWidth = 29;
	int nativeHeight = 26;
	if (panel.panelButtonSkin == "CheckBoxSkin") { nativeWidth = 23; nativeHeight = 21; }
	else if (panel.panelButtonSkin == "RadioButtonSkin") { nativeWidth = 21; nativeHeight = 20; }
	else if (panel.panelButtonSkin == "ButtonCloseSkin") { nativeWidth = 18; nativeHeight = 17; }
	else if (panel.panelButtonSkin == "EditBoxSkin") { nativeWidth = 29; nativeHeight = 26; }
	else if (panel.panelButtonSkin == "MenuBarSkin") { nativeWidth = 27; nativeHeight = 26; }
	else if (panel.panelButtonSkin == "MenuItemSkin") { nativeWidth = 25; nativeHeight = 20; }
	else if (panel.panelButtonSkin == "ListBoxItemSkin") { nativeWidth = 43; nativeHeight = 10; }
	else if (panel.panelButtonSkin == "ClientDefaultSkin") { nativeWidth = 66; nativeHeight = 59; }
	else if (panel.panelButtonSkin == "ClientTileSkin") { nativeWidth = 34; nativeHeight = 18; }
	else if (panel.panelButtonSkin == "ScrollPanelHSkin") { nativeWidth = 19; nativeHeight = 15; }
	else if (panel.panelButtonSkin == "ScrollPanelVSkin") { nativeWidth = 15; nativeHeight = 19; }
	else if (panel.panelButtonSkin == "PanelSkin") { nativeWidth = 23; nativeHeight = 22; }
	else if (panel.panelButtonSkin == "CaptionEmptySkin" || panel.panelButtonSkin == "CaptionSkin" || panel.panelButtonSkin == "CaptionWithButtonSkin") { nativeWidth = 68; nativeHeight = 28; }
	else if (panel.panelButtonSkin == "WindowFrameSkin") { nativeWidth = 23; nativeHeight = 20; }
	else if (panel.panelButtonSkin == "TabHeaderButtonSkin") { nativeWidth = 52; nativeHeight = 23; }
	else if (panel.panelButtonSkin == "TabPanelSkin") { nativeWidth = 43; nativeHeight = 39; }
	else if (panel.panelButtonSkin == "MenuItemNormalSkin") { nativeWidth = 43; nativeHeight = 10; }
	else if (panel.panelButtonSkin == "MultiListButtonSkin") { nativeWidth = 32; nativeHeight = 21; }
	if (panel.panelButtonWidth <= 0 || panel.panelButtonHeight <= 0)
	{
		panel.panelButtonWidth = std::max(1, static_cast<int>(std::lround(nativeWidth * panel.panelButtonScale)));
		panel.panelButtonHeight = std::max(1, static_cast<int>(std::lround(nativeHeight * panel.panelButtonScale)));
	}
	std::vector<GameGUIWidgetDef*> buttons;
	for (GameGUIWidgetDef& widget : asset.widgets)
	{
		if (widget.type == "Button" && widget.parentName == panel.name) buttons.push_back(&widget);
	}
	for (GameGUIWidgetDef* button : buttons)
	{
		button->width = std::max(1, panel.panelButtonWidth);
		button->height = std::max(1, panel.panelButtonHeight);
		button->textColor = panel.panelButtonTextColor;
		button->fontName = panel.panelButtonFontName;
		button->fontSize = panel.panelButtonFontSize;
	}
	if (!panel.uniformButtonSpacing || buttons.empty()) return;

	// Panel padding is a 0-100 spacing compression value. At 100, buttons touch;
	// at 0, the available panel space is distributed between the buttons.
	const float spacingFactor = 1.0f - std::clamp(static_cast<float>(panel.panelPadding) / 100.0f, 0.0f, 1.0f);
	if (panel.horizontalButtonLayout)
	{
		int totalWidth = 0;
		for (const GameGUIWidgetDef* button : buttons) totalWidth += button->width;
		const int availableSpace = std::max(0, panel.width - totalWidth);
		const int gap = panel.panelPadding >= 100
			? -6
			: buttons.size() > 1 ? static_cast<int>(std::lround((availableSpace / static_cast<float>(buttons.size() - 1)) * spacingFactor)) : 0;
		const int groupWidth = totalWidth + gap * static_cast<int>(buttons.size() - 1);
		int x = std::max(0, (panel.width - groupWidth) / 2);
		for (GameGUIWidgetDef* button : buttons) { button->x = x; button->y = (panel.height - button->height) / 2; x += button->width + gap; }
	}
	else
	{
		int totalHeight = 0;
		for (const GameGUIWidgetDef* button : buttons) totalHeight += button->height;
		const int availableSpace = std::max(0, panel.height - totalHeight);
		const int gap = panel.panelPadding >= 100
			? -6
			: buttons.size() > 1 ? static_cast<int>(std::lround((availableSpace / static_cast<float>(buttons.size() - 1)) * spacingFactor)) : 0;
		const int groupHeight = totalHeight + gap * static_cast<int>(buttons.size() - 1);
		int y = std::max(0, (panel.height - groupHeight) / 2);
		for (GameGUIWidgetDef* button : buttons) { button->x = (panel.width - button->width) / 2; button->y = y; y += button->height + gap; }
	}
}

void GameGUICreator::DeleteSelectedWidget()
{
	GameGUIAsset& asset = CurrentRoleGUI();
	if (m_selectedWidgetIndex < 0 || m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		return;
	}

	const std::string deletedName = asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)].name;
	for (GameGUIWidgetDef& widget : asset.widgets) if (widget.parentName == deletedName) widget.parentName.clear();
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




