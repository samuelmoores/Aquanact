#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/UI/EngineGUI.h"

#include <algorithm>
#include <fstream>
#include <sstream>

void GameGUICreator::SaveAllRoleGUIs()
{
	for (std::size_t i = 0; i < m_assets.size(); ++i)
	{
		m_selectedGUI = static_cast<GUIRole>(i);
		SaveSelectedRoleGUI();
	}
}

void GameGUICreator::SaveSelectedRoleGUI()
{
	GameGUIAsset& asset = CurrentRoleGUI();
	for (GameGUIWidgetDef& widget : asset.widgets)
	{
		if (widget.type != "Button" || widget.text.empty() || widget.name == widget.text) continue;
		const std::string previousName = widget.name;
		widget.name = widget.text;
		for (GameGUIWidgetDef& other : asset.widgets) if (other.parentName == previousName) other.parentName = widget.name;
	}
	asset.boxSkin = m_boxSkinIndex == 0 ? "WindowFrameSkin" : m_boxSkinIndex == 1 ? "PanelSkin" : m_boxSkinIndex == 2 ? "ButtonSkin" : m_boxSkinIndex == 3 ? "ButtonEmptySkin" : m_boxSkinIndex == 4 ? "TabPanelSkin" : "ClientDefaultSkin";
	asset.navigationMode = m_menuNavigationMode;
	asset.boxPadding = m_boxPadding;
	asset.boxOffsetX = m_boxOffsetX;
	asset.boxOffsetY = m_boxOffsetY;
	asset.pointerWidth = m_pointerWidth;
	asset.pointerHeight = m_pointerHeight;
	asset.pointerGap = m_pointerGap;
	asset.highlightR = m_highlightR;
	asset.highlightG = m_highlightG;
	asset.highlightB = m_highlightB;
	asset.selectedR = m_selectedR; asset.selectedG = m_selectedG; asset.selectedB = m_selectedB;
	asset.selectedR = m_selectedR; asset.selectedG = m_selectedG; asset.selectedB = m_selectedB;
	const std::filesystem::path projectAssetPath = GUIPathFor(asset);
	const std::filesystem::path executableAssetPath =
		Root::Current().FileSystemRef().ExecutableDirectory() / "assets" / "gameGUI" / (asset.name + ".json");

	std::ostringstream json;
	json << "{\n";
	json << "  \"name\": \"" << asset.name << "\",\n";
	json << "  \"navigationMode\": \"" << (m_menuNavigationMode == MenuNavigationMode::TextHighlight ? "TextHighlight" : m_menuNavigationMode == MenuNavigationMode::Boxed ? "Boxed" : "Pointer") << "\",\n";
	json << "  \"boxSkin\": \"" << (m_boxSkinIndex == 0 ? "WindowFrameSkin" : m_boxSkinIndex == 1 ? "PanelSkin" : m_boxSkinIndex == 2 ? "ButtonSkin" : m_boxSkinIndex == 3 ? "ButtonEmptySkin" : m_boxSkinIndex == 4 ? "TabPanelSkin" : "ClientDefaultSkin") << "\",\n";
	const char* pointerSkins[] = { "NavigationArrowRight1", "NavigationArrowRight2", "NavigationArrowRight3", "NavigationArrowRight4" };
	asset.pointerSkin = pointerSkins[m_pointerSkinIndex];
	json << "  \"pointerSkin\": \"" << pointerSkins[m_pointerSkinIndex] << "\",\n";
	json << "  \"boxPadding\": " << asset.boxPadding << ",\n";
	json << "  \"boxOffsetX\": " << asset.boxOffsetX << ",\n";
	json << "  \"boxOffsetY\": " << asset.boxOffsetY << ",\n";
	json << "  \"pointerWidth\": " << asset.pointerWidth << ",\n";
	json << "  \"pointerHeight\": " << asset.pointerHeight << ",\n";
	json << "  \"pointerGap\": " << asset.pointerGap << ",\n";
	json << "  \"highlightR\": " << asset.highlightR << ",\n";
	json << "  \"highlightG\": " << asset.highlightG << ",\n";
	json << "  \"highlightB\": " << asset.highlightB << ",\n";
	json << "  \"selectedR\": " << asset.selectedR << ",\n";
	json << "  \"selectedG\": " << asset.selectedG << ",\n";
	json << "  \"selectedB\": " << asset.selectedB << ",\n";
	json << "  \"selectedR\": " << asset.selectedR << ",\n";
	json << "  \"selectedG\": " << asset.selectedG << ",\n";
	json << "  \"selectedB\": " << asset.selectedB << ",\n";
	json << "  \"widgets\": [\n";
	for (std::size_t i = 0; i < asset.widgets.size(); ++i)
	{
		const GameGUIWidgetDef& widget = asset.widgets[i];
		json << "    {\n";
		json << "      \"type\": \"" << widget.type << "\",\n";
		json << "      \"name\": \"" << widget.name << "\",\n";
		json << "      \"parent\": \"" << widget.parentName << "\",\n";
		json << "      \"skin\": \"" << widget.skin << "\",\n";
		json << "      \"useSkin\": " << (widget.useSkin ? "true" : "false") << ",\n";
		json << "      \"uniformButtonSpacing\": " << (widget.uniformButtonSpacing ? "true" : "false") << ",\n";
		json << "      \"panelButtonUseSkin\": " << (widget.panelButtonUseSkin ? "true" : "false") << ",\n";
		json << "      \"panelButtonSkin\": \"" << widget.panelButtonSkin << "\",\n";
		json << "      \"panelButtonScale\": " << widget.panelButtonScale << ",\n";
		json << "      \"horizontalButtonLayout\": " << (widget.horizontalButtonLayout ? "true" : "false") << ",\n";
		json << "      \"panelPadding\": " << widget.panelPadding << ",\n";
		json << "      \"panelButtonWidth\": " << widget.panelButtonWidth << ",\n";
		json << "      \"panelButtonHeight\": " << widget.panelButtonHeight << ",\n";
		json << "      \"panelButtonTextColor\": \"" << widget.panelButtonTextColor << "\",\n";
		json << "      \"panelButtonFontName\": \"" << widget.panelButtonFontName << "\",\n";
		json << "      \"panelButtonFontSize\": " << widget.panelButtonFontSize << ",\n";
		json << "      \"text\": \"" << widget.text << "\",\n";
		json << "      \"textColor\": \"" << widget.textColor << "\",\n";
		json << "      \"texture\": \"" << widget.texture << "\",\n";
		json << "      \"layer\": \"" << widget.layer << "\",\n";
		json << "      \"x\": " << widget.x << ",\n";
		json << "      \"y\": " << widget.y << ",\n";
		json << "      \"width\": " << widget.width << ",\n";
		json << "      \"height\": " << widget.height << ",\n";
		json << "      \"textureWidth\": " << widget.textureWidth << ",\n";
		json << "      \"textureHeight\": " << widget.textureHeight << ",\n";
		json << "      \"fontSize\": " << widget.fontSize << ",\n";
		json << "      \"fontName\": \"" << widget.fontName << "\",\n";
		json << "      \"visible\": " << (widget.visible ? "true" : "false") << ",\n";
			json << "      \"alpha\": " << widget.alpha << ",\n";
		json << "      \"highlightColor\": \"" << widget.highlightColor << "\",\n";
		json << "      \"clickedColor\": \"" << widget.clickedColor << "\",\n";
		json << "      \"action\": \"" << GameGUICreatorHelpers::ActionToString(widget.action) << "\",\n";
		json << "      \"launchLevel\": \"" << widget.launchLevel << "\",\n";
		json << "      \"bindEntity\": \"" << widget.bindEntity << "\",\n";
		json << "      \"bindComponent\": \"" << widget.bindComponent << "\",\n";
		json << "      \"bindMember\": \"" << widget.bindMember << "\",\n";
		json << "      \"bindEvent\": \"" << widget.bindEvent << "\"\n";
		json << "    }" << (i + 1 < asset.widgets.size() ? "," : "") << "\n";
	}
	json << "  ]\n";
	json << "}\n";

	const auto writeAsset = [&json](const std::filesystem::path& assetPath) -> bool
	{
		std::error_code ec;
		std::filesystem::create_directories(assetPath.parent_path(), ec);
		if (ec)
		{
			return false;
		}

		std::ofstream file(assetPath, std::ios::trunc);
		if (!file.is_open())
		{
			return false;
		}
		file << json.str();
		return file.good();
	};

	const bool projectSaved = writeAsset(projectAssetPath);
	const bool executableSaved = projectAssetPath == executableAssetPath
		? projectSaved
		: writeAsset(executableAssetPath);
	asset.savedOnDisk = projectSaved && executableSaved;

	if (!projectSaved)
	{
		Root::Current().Debugger().LogMessage("Failed to save project GUI asset: " + projectAssetPath.string());
	}
	else
	{
		Root::Current().Debugger().LogMessage("Saved project GUI asset: " + projectAssetPath.string());
	}
	if (!executableSaved)
	{
		Root::Current().Debugger().LogMessage("Failed to save executable GUI asset: " + executableAssetPath.string());
	}
	else if (projectAssetPath != executableAssetPath)
	{
		Root::Current().Debugger().LogMessage("Saved executable GUI asset: " + executableAssetPath.string());
	}
}

void GameGUICreator::LoadSelectedRoleGUI()
{
	GameGUIAsset& asset = CurrentRoleGUI();
	asset.widgets.clear();
	const std::filesystem::path assetPath = GUIPathFor(asset);
	std::ifstream file(assetPath);
	if (!file.is_open())
	{
		asset.savedOnDisk = false;
		return;
	}

	GameGUIAsset loadedAsset = GameGUICreatorHelpers::LoadAssetFile(assetPath);
	asset = std::move(loadedAsset);
	const std::string skins[] = { "WindowFrameSkin", "PanelSkin", "ButtonSkin", "ButtonEmptySkin", "TabPanelSkin", "ClientDefaultSkin" };
	m_boxSkinIndex = 0;
	for (int i = 0; i < 6; ++i) if (asset.boxSkin == skins[i]) { m_boxSkinIndex = i; break; }
	const std::string pointerSkins[] = { "NavigationArrowRight1", "NavigationArrowRight2", "NavigationArrowRight3", "NavigationArrowRight4" };
	m_pointerSkinIndex = 0;
	for (int i = 0; i < 4; ++i) if (asset.pointerSkin == pointerSkins[i]) { m_pointerSkinIndex = i; break; }
	LoadNavigationSettingsFromAsset();
	m_selectedWidgetIndex = asset.widgets.empty() ? -1 : 0;
}

void GameGUICreator::SyncRuntimePreview()
{
	auto& runtimeGUI = Root::Current().FrontEnd().RuntimeGUI();
	runtimeGUI.LoadPreviewAsset(CurrentRoleGUI());
	if (IsMainMenuSelected())
	{
		// Give the creator preview an explicit highlighted button so the menu
		// pointer is visible without requiring controller input.
		runtimeGUI.FocusFirstControllerButton();
	}
}

void GameGUICreator::PreviewSelectedGUI()
{
	if (m_initialized)
	{
		SyncRuntimePreview();
	}
}

void GameGUICreator::RestoreEditorViewState()
{
	if (!m_previousViewStateCaptured)
	{
		return;
	}

	EngineGUI& engineGUI = Root::Current().FrontEnd().EditorGUI();
	engineGUI.SetShowAxis(m_previousShowAxis);
	engineGUI.SetShowGrid(m_previousShowGrid);
	m_previousViewStateCaptured = false;
}

