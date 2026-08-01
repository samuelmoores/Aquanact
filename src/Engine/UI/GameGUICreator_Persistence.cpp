#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/EngineGUI.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
	std::string ActionToString(GameGUIActionType action)
	{
		switch (action)
		{
		case GameGUIActionType::NewGame:
			return "NewGame";
		default:
			return "None";
		}
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

		std::size_t widgetPos = contents.find("\"type\": \"");
		while (widgetPos != std::string::npos)
		{
			GameGUIWidgetDef widget;
			const auto readField = [&contents](const std::string& key, std::size_t start) -> std::string
			{
				const std::size_t keyPos = contents.find(key, start);
				if (keyPos == std::string::npos)
				{
					return {};
				}
				const std::size_t valueStart = contents.find_first_not_of(" \t", keyPos + key.size());
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
			};

			auto readInt = [](const std::string& value, int fallback = 0)
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
			};

			widget.type = readField("\"type\":", widgetPos);
			widget.name = readField("\"name\":", widgetPos);
			widget.parentName = readField("\"parent\":", widgetPos);
			widget.skin = readField("\"skin\":", widgetPos);
			widget.text = readField("\"text\":", widgetPos);
			widget.texture = readField("\"texture\":", widgetPos);
			widget.layer = readField("\"layer\":", widgetPos);
			widget.x = readInt(readField("\"x\":", widgetPos));
			widget.y = readInt(readField("\"y\":", widgetPos));
			widget.width = readInt(readField("\"width\":", widgetPos), 100);
			widget.height = readInt(readField("\"height\":", widgetPos), 30);
			widget.fontSize = readInt(readField("\"fontSize\":", widgetPos), 0);
			widget.visible = readField("\"visible\":", widgetPos).find("true") != std::string::npos;
			widget.alpha = 1.0f;
			try { widget.alpha = std::stof(readField("\"alpha\":", widgetPos)); } catch (...) {}
			widget.action = readField("\"action\":", widgetPos) == "NewGame" ? GameGUIActionType::NewGame : GameGUIActionType::None;
			widget.bindEntity = readField("\"bindEntity\":", widgetPos);
			widget.bindComponent = readField("\"bindComponent\":", widgetPos);
			widget.bindMember = readField("\"bindMember\":", widgetPos);
			widget.bindEvent = readField("\"bindEvent\":", widgetPos);
			asset.widgets.push_back(widget);
			widgetPos = contents.find("\"type\": \"", widgetPos + 1);
		}

		return asset;
	}
}

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
	const std::filesystem::path assetPath = GUIPathFor(asset);
	const std::filesystem::path directory = assetPath.parent_path();
	std::error_code ec;
	std::filesystem::create_directories(directory, ec);

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
		json << "      \"action\": \"" << ActionToString(widget.action) << "\",\n";
		json << "      \"bindEntity\": \"" << widget.bindEntity << "\",\n";
		json << "      \"bindComponent\": \"" << widget.bindComponent << "\",\n";
		json << "      \"bindMember\": \"" << widget.bindMember << "\",\n";
		json << "      \"bindEvent\": \"" << widget.bindEvent << "\"\n";
		json << "    }" << (i + 1 < asset.widgets.size() ? "," : "") << "\n";
	}
	json << "  ]\n";
	json << "}\n";

	std::ofstream file(assetPath, std::ios::trunc);
	if (!file.is_open())
	{
		Root::Current().Debugger().LogMessage("Failed to save GUI asset: " + assetPath.string());
		return;
	}

	file << json.str();
	asset.savedOnDisk = true;
	Root::Current().Debugger().LogMessage("Saved GUI asset: " + assetPath.string());
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

	GameGUIAsset loadedAsset = LoadAssetFile(assetPath);
	asset = std::move(loadedAsset);
	m_selectedWidgetIndex = asset.widgets.empty() ? -1 : 0;
}

void GameGUICreator::SyncRuntimePreview()
{
	Root::Current().FrontEnd().RuntimeGUI().LoadUIAsset(CurrentRoleGUI());
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

