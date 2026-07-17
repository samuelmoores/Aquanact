#include "GameGUICreator.h"

#include "FrontEndManager.h"
#include "RenderManager.h"
#include "Debug.h"
#include "Window.h"
#include "Camera.h"
#include "Globals.h"

#include <imgui.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace {
	std::filesystem::path AssetDirectory()
	{
#ifdef AQUANACT_SOURCE_ROOT
		// UI assets are authored in the source tree, not the build tree, so the
		// editor can save and reload them without depending on build output.
		return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "assets" / "gameGUI";
#else
		return std::filesystem::current_path() / "assets" / "gameGUI";
#endif
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

			widget.type = readField("\"type\":", widgetPos);
			widget.name = readField("\"name\":", widgetPos);
			widget.skin = readField("\"skin\":", widgetPos);
			widget.text = readField("\"text\":", widgetPos);
			widget.layer = readField("\"layer\":", widgetPos);
			widget.x = std::stoi(readField("\"x\":", widgetPos));
			widget.y = std::stoi(readField("\"y\":", widgetPos));
			widget.width = std::stoi(readField("\"width\":", widgetPos));
			widget.height = std::stoi(readField("\"height\":", widgetPos));
			widget.visible = readField("\"visible\":", widgetPos).find("true") != std::string::npos;
			widget.alpha = std::stof(readField("\"alpha\":", widgetPos));
			asset.widgets.push_back(widget);
			widgetPos = contents.find("\"type\": \"", widgetPos + 1);
		}

		return asset;
	}
}

void GameGUICreator::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	m_window = &window;
	m_assets.clear();
	const std::filesystem::path assetDirectory = AssetDirectory();
	std::error_code ec;
	if (std::filesystem::exists(assetDirectory, ec) && !ec)
	{
		for (const auto& entry : std::filesystem::directory_iterator(assetDirectory, ec))
		{
			if (ec)
			{
				break;
			}
			if (!entry.is_regular_file() || entry.path().extension() != ".json")
			{
				continue;
			}
			m_assets.push_back(LoadAssetFile(entry.path()));
		}
	}
	m_selectedAssetIndex = -1;
	m_selectedWidgetIndex = -1;
	m_newAssetName[0] = '\0';
	m_initialized = true;
	if (!m_assets.empty())
	{
		m_selectedAssetIndex = 0;
		m_selectedWidgetIndex = m_assets.front().widgets.empty() ? -1 : 0;
	}
	SyncRuntimePreview();
}

void GameGUICreator::shutDown()
{
	m_window = nullptr;
	m_initialized = false;
	m_showCreateAssetPopup = false;
	m_newAssetName[0] = '\0';
	m_assets.clear();
	m_selectedAssetIndex = -1;
	m_selectedWidgetIndex = -1;
}

void GameGUICreator::BeginFrame()
{
}

GameGUIAsset& GameGUICreator::CurrentAsset()
{
	return m_assets[static_cast<std::size_t>(m_selectedAssetIndex)];
}

const GameGUIAsset& GameGUICreator::CurrentAsset() const
{
	return m_assets[static_cast<std::size_t>(m_selectedAssetIndex)];
}

std::filesystem::path GameGUICreator::AssetPathFor(const GameGUIAsset& asset) const
{
	return AssetDirectory() / (asset.name + ".json");
}

void GameGUICreator::Draw(const Camera&)
{
	if (!m_initialized)
	{
		return;
	}

	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		m_selectedAssetIndex = m_assets.empty() ? -1 : 0;
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Create Asset"))
			{
				m_showCreateAssetPopup = true;
				m_newAssetName[0] = '\0';
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Create Button", nullptr, false, m_selectedAssetIndex >= 0))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetName[0] = '\0';
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save UI", nullptr, false, m_selectedAssetIndex >= 0))
			{
				SaveCurrentAsset();
				gDebug.LogMessage("Save UI requested");
			}
			if (ImGui::MenuItem("Load UI", nullptr, false, m_selectedAssetIndex >= 0))
			{
				// Load now means "reload the selected asset from disk" after startup scan.
				LoadCurrentAsset();
				SyncRuntimePreview();
				gDebug.LogMessage("Load UI requested");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("UI"))
		{
			if (ImGui::MenuItem("Leave UI Creation"))
			{
				gFrontEndManager.ReturnToEngineGUIEditor();
				gRenderManager.SetActiveCamera(gRenderManager.GetEngineCamera());
				gDebug.LogMessage("Leave UI Creation requested");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			EngineGUI& engineGUI = gFrontEndManager.EditorGUI();
			bool showAxis = engineGUI.ShowAxis();
			bool showGrid = engineGUI.ShowGrid();
			ImGui::MenuItem("Axis", nullptr, &showAxis);
			if (showAxis != engineGUI.ShowAxis())
			{
				engineGUI.SetShowAxis(showAxis);
			}
			ImGui::Separator();
			ImGui::MenuItem("Show Grid", nullptr, &showGrid);
			if (showGrid != engineGUI.ShowGrid())
			{
				engineGUI.SetShowGrid(showGrid);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	DrawCreateAssetPopup();
	DrawCreateWidgetPopup();

		ImGui::Begin("UIAssets");
	for (std::size_t i = 0; i < m_assets.size(); ++i)
	{
		const GameGUIAsset& asset = m_assets[i];
		const bool selected = m_selectedAssetIndex == static_cast<int>(i);
		if (ImGui::Selectable(asset.name.c_str(), selected))
		{
			m_selectedAssetIndex = static_cast<int>(i);
			m_selectedWidgetIndex = asset.widgets.empty() ? -1 : 0;
			SyncRuntimePreview();
		}
	}
	if (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(m_assets.size()))
	{
		const GameGUIAsset& asset = CurrentAsset();
		ImGui::Separator();
		ImGui::Text("Asset: %s", asset.name.c_str());
		ImGui::Text("Saved on disk: %s", asset.savedOnDisk ? "yes" : "no");
		if (ImGui::Button("Delete Asset"))
		{
			DeleteSelectedAsset();
		}
	}
	ImGui::End();

	ImGui::Begin("Widgets");
	if (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(m_assets.size()))
	{
		GameGUIAsset& asset = CurrentAsset();
		for (std::size_t i = 0; i < asset.widgets.size(); ++i)
		{
			const GameGUIWidgetDef& widget = asset.widgets[i];
			const bool selected = m_selectedWidgetIndex == static_cast<int>(i);
			if (ImGui::Selectable(widget.name.c_str(), selected))
			{
				m_selectedWidgetIndex = static_cast<int>(i);
			}
		}
		if (m_selectedWidgetIndex >= 0 && m_selectedWidgetIndex < static_cast<int>(asset.widgets.size()))
		{
			GameGUIWidgetDef& widget = asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)];
			ImGui::Separator();
			ImGui::Text("Name: %s", widget.name.c_str());
			ImGui::Text("Type: %s", widget.type.c_str());
			ImGui::Text("Position: %d, %d", widget.x, widget.y);
			ImGui::Text("Size: %d x %d", widget.width, widget.height);
			float position[2] = { static_cast<float>(widget.x), static_cast<float>(widget.y) };
			if (ImGui::DragFloat2("Move", position, 1.0f))
			{
				widget.x = static_cast<int>(position[0]);
				widget.y = static_cast<int>(position[1]);
				SyncRuntimePreview();
			}
			if (ImGui::Button("Delete Widget"))
			{
				DeleteSelectedWidget();
			}
		}
	}
	ImGui::End();
}

void GameGUICreator::EndFrame()
{
}

void GameGUICreator::DrawCreateAssetPopup()
{
	if (m_showCreateAssetPopup)
	{
		ImGui::OpenPopup("Create Asset");
		m_showCreateAssetPopup = false;
	}

	if (ImGui::BeginPopupModal("Create Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Enter a UI asset name:");
		ImGui::InputText("Name", m_newAssetName, sizeof(m_newAssetName));
		if (ImGui::Button("Create"))
		{
			std::string name = m_newAssetName;
			if (name.empty())
			{
				// Empty names used to auto-fill a default asset, but that created
				// unwanted files. Now we treat it as a cancel operation instead.
				gDebug.LogMessage("Create Asset cancelled: name is empty");
				ImGui::CloseCurrentPopup();
				return;
			}
			AddGameGUIAsset(name);
			SaveCurrentAsset();
			SyncRuntimePreview();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void GameGUICreator::DrawCreateWidgetPopup()
{
	if (m_showCreateWidgetPopup)
	{
		ImGui::OpenPopup("Create Widget");
		m_showCreateWidgetPopup = false;
	}

	if (ImGui::BeginPopupModal("Create Widget", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Enter a widget name:");
		ImGui::InputText("Name", m_newWidgetName, sizeof(m_newWidgetName));
		if (ImGui::Button("Create"))
		{
			AddButtonWidget();
			SyncRuntimePreview();
			gDebug.LogMessage("Create Button requested");
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void GameGUICreator::AddGameGUIAsset(const std::string& name)
{
	GameGUIAsset asset;
	asset.name = name;
	int suffix = 1;
	while (std::any_of(m_assets.begin(), m_assets.end(), [&asset](const GameGUIAsset& existing)
	{
		return existing.name == asset.name;
	}))
	{
		asset.name = name + "_" + std::to_string(++suffix);
	}

	m_assets.push_back(asset);
	m_selectedAssetIndex = static_cast<int>(m_assets.size() - 1);
	m_selectedWidgetIndex = -1;
}

void GameGUICreator::AddButtonWidget()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		gDebug.LogMessage("AddButtonWidget skipped: no active GameGUI asset");
		return;
	}

	GameGUIWidgetDef button;
	button.type = "Button";
	button.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "button";
	button.skin = "ButtonSkin";
	button.text = "Test Button";
	button.layer = "Main";
	button.width = 320;
	button.height = 90;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	button.x = std::max(0, (framebufferWidth - button.width) / 2);
	button.y = std::max(0, (framebufferHeight - button.height) / 2);

	GameGUIAsset& asset = CurrentAsset();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&button](const GameGUIWidgetDef& widget)
	{
		return widget.name == button.name;
	}))
	{
		button.name = "button_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(button);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	m_newWidgetName[0] = '\0';
	gDebug.LogMessage(
		std::string("Created GameGUI button widget: name='") + button.name +
		"', asset='" + asset.name +
		"', pos=(" + std::to_string(button.x) + "," + std::to_string(button.y) + ")" +
		", size=(" + std::to_string(button.width) + "x" + std::to_string(button.height) + ")");
}

void GameGUICreator::SaveCurrentAsset()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	GameGUIAsset& asset = CurrentAsset();
	const std::filesystem::path assetPath = AssetPathFor(asset);
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
		json << "      \"skin\": \"" << widget.skin << "\",\n";
		json << "      \"text\": \"" << widget.text << "\",\n";
		json << "      \"layer\": \"" << widget.layer << "\",\n";
		json << "      \"x\": " << widget.x << ",\n";
		json << "      \"y\": " << widget.y << ",\n";
		json << "      \"width\": " << widget.width << ",\n";
		json << "      \"height\": " << widget.height << ",\n";
		json << "      \"visible\": " << (widget.visible ? "true" : "false") << ",\n";
		json << "      \"alpha\": " << widget.alpha << "\n";
		json << "    }" << (i + 1 < asset.widgets.size() ? "," : "") << "\n";
	}
	json << "  ]\n";
	json << "}\n";

	std::ofstream file(assetPath, std::ios::trunc);
	if (!file.is_open())
	{
		gDebug.LogMessage("Failed to save UI asset: " + assetPath.string());
		return;
	}

	file << json.str();
	asset.savedOnDisk = true;
	gDebug.LogMessage("Saved UI asset: " + assetPath.string());
}

void GameGUICreator::LoadCurrentAsset()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	GameGUIAsset& asset = CurrentAsset();
	asset.widgets.clear();
	const std::filesystem::path assetPath = AssetPathFor(asset);
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

void GameGUICreator::DeleteSelectedWidget()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	GameGUIAsset& asset = CurrentAsset();
	if (m_selectedWidgetIndex < 0 || m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		return;
	}

	asset.widgets.erase(asset.widgets.begin() + m_selectedWidgetIndex);
	if (asset.widgets.empty())
	{
		m_selectedWidgetIndex = -1;
	}
	else if (m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
	{
		m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	}

	SyncRuntimePreview();
}

void GameGUICreator::DeleteSelectedAsset()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	const GameGUIAsset asset = CurrentAsset();
	const std::filesystem::path assetPath = AssetPathFor(asset);
	if (std::filesystem::exists(assetPath))
	{
		std::error_code ec;
		std::filesystem::remove(assetPath, ec);
		if (ec)
		{
			gDebug.LogMessage("Failed to delete UI asset: " + assetPath.string());
			return;
		}
	}

	m_assets.erase(m_assets.begin() + m_selectedAssetIndex);
	if (m_assets.empty())
	{
		m_selectedAssetIndex = -1;
		m_selectedWidgetIndex = -1;
		gFrontEndManager.RuntimeGUI().ClearUI();
		return;
	}

	if (m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		m_selectedAssetIndex = static_cast<int>(m_assets.size() - 1);
	}
	m_selectedWidgetIndex = m_assets[static_cast<std::size_t>(m_selectedAssetIndex)].widgets.empty() ? -1 : 0;
	SyncRuntimePreview();
}

void GameGUICreator::SyncRuntimePreview()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		gFrontEndManager.RuntimeGUI().ClearUI();
		return;
	}

	gFrontEndManager.RuntimeGUI().LoadUIAsset(CurrentAsset());
}

bool GameGUICreator::IsCurrentAssetStoredOnDisk() const
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return false;
	}

	return std::filesystem::exists(AssetPathFor(CurrentAsset()));
}
