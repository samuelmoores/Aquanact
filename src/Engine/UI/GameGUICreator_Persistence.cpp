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
	const std::filesystem::path projectAssetPath = GUIPathFor(asset);
	const std::filesystem::path executableAssetPath =
		Root::Current().FileSystemRef().ExecutableDirectory() / "assets" / "gameGUI" / (asset.name + ".json");

	std::ostringstream json;
	json << "{\n";
	json << "  \"name\": \"" << asset.name << "\",\n";
	json << "  \"widgets\": [\n";
	for (std::size_t i = 0; i < asset.widgets.size(); ++i)
	{
		const GameGUIWidgetDef& widget = asset.widgets[i];
		json << "    {\n";
		json << "      \"type\": \"" << widget.type << "\",\n";
		json << "      \"name\": \"" << widget.name << "\",\n";
		json << "      \"parent\": \"" << widget.parentName << "\",\n";
		json << "      \"skin\": \"" << widget.skin << "\",\n";
		json << "      \"text\": \"" << widget.text << "\",\n";
		json << "      \"texture\": \"" << widget.texture << "\",\n";
		json << "      \"layer\": \"" << widget.layer << "\",\n";
		json << "      \"x\": " << widget.x << ",\n";
		json << "      \"y\": " << widget.y << ",\n";
		json << "      \"width\": " << widget.width << ",\n";
		json << "      \"height\": " << widget.height << ",\n";
		json << "      \"textureWidth\": " << widget.textureWidth << ",\n";
		json << "      \"textureHeight\": " << widget.textureHeight << ",\n";
		json << "      \"fontSize\": " << widget.fontSize << ",\n";
		json << "      \"visible\": " << (widget.visible ? "true" : "false") << ",\n";
			json << "      \"alpha\": " << widget.alpha << ",\n";
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
	m_selectedWidgetIndex = asset.widgets.empty() ? -1 : 0;
}

void GameGUICreator::SyncRuntimePreview()
{
	Root::Current().FrontEnd().RuntimeGUI().LoadPreviewAsset(CurrentRoleGUI());
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

