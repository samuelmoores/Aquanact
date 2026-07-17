#include "UICreator.h"

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

namespace {
	std::filesystem::path AssetDirectory()
	{
		return std::filesystem::current_path() / "UI";
	}
}

void UICreator::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	m_window = &window;
	m_assets.clear();
	m_selectedAssetIndex = -1;
	m_selectedWidgetIndex = -1;
	m_newAssetName[0] = '\0';
	m_initialized = true;
	SyncRuntimePreview();
}

void UICreator::shutDown()
{
	m_window = nullptr;
	m_initialized = false;
	m_showCreateAssetPopup = false;
	m_newAssetName[0] = '\0';
	m_assets.clear();
	m_selectedAssetIndex = -1;
	m_selectedWidgetIndex = -1;
}

void UICreator::BeginFrame()
{
}

UIAsset& UICreator::CurrentAsset()
{
	return m_assets[static_cast<std::size_t>(m_selectedAssetIndex)];
}

const UIAsset& UICreator::CurrentAsset() const
{
	return m_assets[static_cast<std::size_t>(m_selectedAssetIndex)];
}

std::filesystem::path UICreator::AssetPathFor(const UIAsset& asset) const
{
	return AssetDirectory() / (asset.name + ".json");
}

void UICreator::Draw(const Camera&)
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
				AddButtonWidget();
				SyncRuntimePreview();
				gDebug.LogMessage("Create Button requested");
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
				gFrontEndManager.ReturnToEngineEditor();
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

		ImGui::Begin("UIAssets");
	for (std::size_t i = 0; i < m_assets.size(); ++i)
	{
		const UIAsset& asset = m_assets[i];
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
		const UIAsset& asset = CurrentAsset();
		ImGui::Separator();
		ImGui::Text("Asset: %s", asset.name.c_str());
		ImGui::Text("Saved on disk: %s", asset.savedOnDisk ? "yes" : "no");
	}
	ImGui::End();

	ImGui::Begin("Widgets");
	if (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(m_assets.size()))
	{
		UIAsset& asset = CurrentAsset();
		for (std::size_t i = 0; i < asset.widgets.size(); ++i)
		{
			const UIWidgetDef& widget = asset.widgets[i];
			const bool selected = m_selectedWidgetIndex == static_cast<int>(i);
			if (ImGui::Selectable(widget.name.c_str(), selected))
			{
				m_selectedWidgetIndex = static_cast<int>(i);
			}
		}
		if (m_selectedWidgetIndex >= 0 && m_selectedWidgetIndex < static_cast<int>(asset.widgets.size()))
		{
			UIWidgetDef& widget = asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)];
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

void UICreator::EndFrame()
{
}

void UICreator::DrawCreateAssetPopup()
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
				name = "HUD";
			}
			AddUIAsset(name);
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

void UICreator::AddUIAsset(const std::string& name)
{
	UIAsset asset;
	asset.name = name;
	int suffix = 1;
	while (std::any_of(m_assets.begin(), m_assets.end(), [&asset](const UIAsset& existing)
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

void UICreator::AddButtonWidget()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	UIWidgetDef button;
	button.type = "Button";
	button.name = "button";
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

	UIAsset& asset = CurrentAsset();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&button](const UIWidgetDef& widget)
	{
		return widget.name == button.name;
	}))
	{
		button.name = "button_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(button);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
}

void UICreator::SaveCurrentAsset()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	UIAsset& asset = CurrentAsset();
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
		const UIWidgetDef& widget = asset.widgets[i];
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

void UICreator::LoadCurrentAsset()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	UIAsset& asset = CurrentAsset();
	asset.widgets.clear();
	const std::filesystem::path assetPath = AssetPathFor(asset);
	std::ifstream file(assetPath);
	if (!file.is_open())
	{
		asset.savedOnDisk = false;
		return;
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
		UIWidgetDef widget;
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

	asset.savedOnDisk = true;
	m_selectedWidgetIndex = asset.widgets.empty() ? -1 : 0;
}

void UICreator::DeleteSelectedWidget()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	UIAsset& asset = CurrentAsset();
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

void UICreator::DeleteSelectedAsset()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return;
	}

	const UIAsset asset = CurrentAsset();
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

void UICreator::SyncRuntimePreview()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		gFrontEndManager.RuntimeGUI().ClearUI();
		return;
	}

	gFrontEndManager.RuntimeGUI().LoadUIAsset(CurrentAsset());
}

bool UICreator::IsCurrentAssetStoredOnDisk() const
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		return false;
	}

	return std::filesystem::exists(AssetPathFor(CurrentAsset()));
}
