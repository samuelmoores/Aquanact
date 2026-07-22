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
#include <functional>

namespace {
	const char* ActionLabel(GameGUIActionType action)
	{
		switch (action)
		{
		case GameGUIActionType::QuitGame:
			return "Quit Game";
		case GameGUIActionType::PauseGame:
			return "Pause Game";
		default:
			return "None";
		}
	}

	std::string ActionToString(GameGUIActionType action)
	{
		switch (action)
		{
		case GameGUIActionType::QuitGame:
			return "QuitGame";
		case GameGUIActionType::PauseGame:
			return "PauseGame";
		default:
			return "None";
		}
	}

	GameGUIActionType StringToAction(const std::string& value)
	{
		if (value == "QuitGame")
		{
			return GameGUIActionType::QuitGame;
		}
		if (value == "PauseGame")
		{
			return GameGUIActionType::PauseGame;
		}
		return GameGUIActionType::None;
	}

	bool WouldCreateParentCycle(const GameGUIAsset& asset, const std::string& childName, const std::string& parentName)
	{
		if (childName.empty() || parentName.empty())
		{
			return false;
		}
		if (childName == parentName)
		{
			return true;
		}

		std::string currentParent = parentName;
		while (!currentParent.empty())
		{
			if (currentParent == childName)
			{
				return true;
			}

			auto it = std::find_if(asset.widgets.begin(), asset.widgets.end(), [&currentParent](const GameGUIWidgetDef& widget)
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
			widget.parentName = readField("\"parent\":", widgetPos);
			widget.skin = readField("\"skin\":", widgetPos);
			widget.text = readField("\"text\":", widgetPos);
			widget.texture = readField("\"texture\":", widgetPos);
			widget.layer = readField("\"layer\":", widgetPos);
			widget.x = std::stoi(readField("\"x\":", widgetPos));
			widget.y = std::stoi(readField("\"y\":", widgetPos));
			widget.width = std::stoi(readField("\"width\":", widgetPos));
			widget.height = std::stoi(readField("\"height\":", widgetPos));
			widget.visible = readField("\"visible\":", widgetPos).find("true") != std::string::npos;
			widget.alpha = std::stof(readField("\"alpha\":", widgetPos));
			widget.action = StringToAction(readField("\"action\":", widgetPos));
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
	m_newWidgetName[0] = '\0';
	m_newWidgetText[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	m_newWidgetIsText = false;
	m_newWidgetIsImage = false;
	m_newWidgetAction = GameGUIActionType::None;
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
	m_newWidgetName[0] = '\0';
	m_newWidgetText[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	m_newWidgetIsText = false;
	m_newWidgetIsImage = false;
	m_newWidgetAction = GameGUIActionType::None;
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
		if (ImGui::BeginMenu("UI"))
		{
			if (ImGui::MenuItem("Leave GameGUI Creator"))
			{
				gFrontEndManager.ReturnToEngineGUIEditor();
				gRenderManager.SetActiveCamera(gRenderManager.GetEngineCamera());
				gDebug.LogMessage("Leave GameGUI Creator requested");
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
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Create GameGUI Asset"))
			{
				m_showCreateAssetPopup = true;
				m_newAssetName[0] = '\0';
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Create Button", nullptr, false, m_selectedAssetIndex >= 0))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsText = false;
				m_newWidgetIsImage = false;
				m_newWidgetName[0] = '\0';
				m_newWidgetText[0] = '\0';
				m_newWidgetTexture[0] = '\0';
			}
			if (ImGui::MenuItem("Create Text", nullptr, false, m_selectedAssetIndex >= 0))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsText = true;
				m_newWidgetIsImage = false;
				m_newWidgetName[0] = '\0';
				std::snprintf(m_newWidgetText, sizeof(m_newWidgetText), "New Text");
				m_newWidgetTexture[0] = '\0';
			}
			if (ImGui::MenuItem("Create Image", nullptr, false, m_selectedAssetIndex >= 0))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsText = false;
				m_newWidgetIsImage = true;
				m_newWidgetName[0] = '\0';
				m_newWidgetText[0] = '\0';
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "textures/example.png");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	DrawCreateAssetPopup();
	DrawCreateWidgetPopup();

		ImGui::Begin("GameGUI Assets");
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
		const auto hasChildren = [&asset](const GameGUIWidgetDef& widget)
		{
			return std::any_of(asset.widgets.begin(), asset.widgets.end(), [&widget](const GameGUIWidgetDef& child)
			{
				return child.parentName == widget.name;
			});
		};

		std::function<void(const std::string&)> drawChildren;
		drawChildren = [&](const std::string& parentName)
		{
			for (std::size_t i = 0; i < asset.widgets.size(); ++i)
			{
				GameGUIWidgetDef& widget = asset.widgets[i];
				if (widget.parentName != parentName)
				{
					continue;
				}

				const bool selected = m_selectedWidgetIndex == static_cast<int>(i);
				const bool widgetHasChildren = hasChildren(widget);
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (selected)
				{
					flags |= ImGuiTreeNodeFlags_Selected;
				}
				if (!widgetHasChildren)
				{
					flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				}

				const std::string label = widget.name + " (" + widget.type + ")##WidgetTree" + std::to_string(i);
				const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					m_selectedWidgetIndex = static_cast<int>(i);
				}

				if (widgetHasChildren && open)
				{
					drawChildren(widget.name);
					ImGui::TreePop();
				}
			}
		};

		drawChildren("");

		if (m_selectedWidgetIndex >= 0 && m_selectedWidgetIndex < static_cast<int>(asset.widgets.size()))
		{
			GameGUIWidgetDef& widget = asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)];
			ImGui::Separator();
			ImGui::Text("Name: %s", widget.name.c_str());
			ImGui::Text("Type: %s", widget.type.c_str());
			const char* parentLabel = widget.parentName.empty() ? "<None>" : widget.parentName.c_str();
			if (ImGui::BeginCombo("Parent", parentLabel))
			{
				const bool noneSelected = widget.parentName.empty();
				if (ImGui::Selectable("<None>", noneSelected))
				{
					widget.parentName.clear();
					SyncRuntimePreview();
				}
				if (noneSelected)
				{
					ImGui::SetItemDefaultFocus();
				}

				for (const GameGUIWidgetDef& parentCandidate : asset.widgets)
				{
					if (parentCandidate.name == widget.name ||
						WouldCreateParentCycle(asset, widget.name, parentCandidate.name))
					{
						continue;
					}

					const bool selected = widget.parentName == parentCandidate.name;
					if (ImGui::Selectable(parentCandidate.name.c_str(), selected))
					{
						widget.parentName = parentCandidate.name;
						SyncRuntimePreview();
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::Text("Position: %d, %d", widget.x, widget.y);
			ImGui::Text("Size: %d x %d", widget.width, widget.height);
			if (widget.type == "ImageBox" || widget.type == "Image")
			{
				char textureBuffer[256] = { 0 };
				std::snprintf(textureBuffer, sizeof(textureBuffer), "%s", widget.texture.c_str());
				if (ImGui::InputText("Texture", textureBuffer, sizeof(textureBuffer)))
				{
					widget.texture = textureBuffer;
					SyncRuntimePreview();
				}
			}
			else
			{
				char textBuffer[128] = { 0 };
				std::snprintf(textBuffer, sizeof(textBuffer), "%s", widget.text.c_str());
				if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer)))
				{
					widget.text = textBuffer;
					SyncRuntimePreview();
				}
			}
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
		ImGui::OpenPopup("Create GameGUI Asset");
		m_showCreateAssetPopup = false;
	}

	if (ImGui::BeginPopupModal("Create GameGUI Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
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
				gDebug.LogMessage("Create GameGUI Asset cancelled: name is empty");
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
		const char* widgetKind = m_newWidgetIsImage ? "Create an image widget:" : (m_newWidgetIsText ? "Create a text widget:" : "Create a button widget:");
		ImGui::TextUnformatted(widgetKind);
		ImGui::InputText("Name", m_newWidgetName, sizeof(m_newWidgetName));
		if (m_newWidgetIsText)
		{
			ImGui::InputText("Text", m_newWidgetText, sizeof(m_newWidgetText));
		}
		else if (m_newWidgetIsImage)
		{
			ImGui::InputText("Texture", m_newWidgetTexture, sizeof(m_newWidgetTexture));
		}
		else
		{
			GameGUIActionType action = m_newWidgetAction;
			if (ImGui::BeginCombo("Action", ActionLabel(action)))
			{
				const GameGUIActionType options[] = { GameGUIActionType::None, GameGUIActionType::QuitGame, GameGUIActionType::PauseGame };
				for (GameGUIActionType option : options)
				{
					const bool selected = option == action;
					if (ImGui::Selectable(ActionLabel(option), selected))
					{
						action = option;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			m_newWidgetAction = action;
		}
		if (ImGui::Button("Create"))
		{
			if (m_newWidgetIsText)
			{
				AddTextWidget();
			}
			else if (m_newWidgetIsImage)
			{
				AddImageWidget();
			}
			else
			{
				AddButtonWidget();
			}
			SyncRuntimePreview();
			gDebug.LogMessage(m_newWidgetIsImage ? "Create Image requested" : (m_newWidgetIsText ? "Create Text requested" : "Create Button requested"));
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
	button.action = m_newWidgetAction;

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
	m_newWidgetAction = GameGUIActionType::None;
	gDebug.LogMessage(
		std::string("Created GameGUI button widget: name='") + button.name +
		"', asset='" + asset.name +
		"', pos=(" + std::to_string(button.x) + "," + std::to_string(button.y) + ")" +
		", size=(" + std::to_string(button.width) + "x" + std::to_string(button.height) + ")");
}

void GameGUICreator::AddTextWidget()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		gDebug.LogMessage("AddTextWidget skipped: no active GameGUI asset");
		return;
	}

	GameGUIWidgetDef text;
	text.type = "TextBox";
	text.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "text";
	text.skin = "TextBox";
	text.text = m_newWidgetText[0] != '\0' ? m_newWidgetText : "New Text";
	text.layer = "Main";
	text.width = 360;
	text.height = 48;
	text.action = GameGUIActionType::None;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	text.x = std::max(0, (framebufferWidth - text.width) / 2);
	text.y = std::max(0, (framebufferHeight - text.height) / 2);

	GameGUIAsset& asset = CurrentAsset();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&text](const GameGUIWidgetDef& widget)
	{
		return widget.name == text.name;
	}))
	{
		text.name = "text_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(text);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	m_newWidgetName[0] = '\0';
	m_newWidgetText[0] = '\0';
	gDebug.LogMessage(
		std::string("Created GameGUI text widget: name='") + text.name +
		"', asset='" + asset.name +
		"', pos=(" + std::to_string(text.x) + "," + std::to_string(text.y) + ")" +
		", size=(" + std::to_string(text.width) + "x" + std::to_string(text.height) + ")");
}

void GameGUICreator::AddImageWidget()
{
	if (m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
	{
		gDebug.LogMessage("AddImageWidget skipped: no active GameGUI asset");
		return;
	}

	GameGUIWidgetDef image;
	image.type = "ImageBox";
	image.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "image";
	image.skin = "ImageBox";
	image.texture = m_newWidgetTexture[0] != '\0' ? m_newWidgetTexture : "textures/example.png";
	image.layer = "Main";
	image.width = 256;
	image.height = 256;
	image.action = GameGUIActionType::None;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	image.x = std::max(0, (framebufferWidth - image.width) / 2);
	image.y = std::max(0, (framebufferHeight - image.height) / 2);

	GameGUIAsset& asset = CurrentAsset();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&image](const GameGUIWidgetDef& widget)
	{
		return widget.name == image.name;
	}))
	{
		image.name = "image_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(image);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	m_newWidgetName[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	gDebug.LogMessage(
		std::string("Created GameGUI image widget: name='") + image.name +
		"', asset='" + asset.name +
		"', texture='" + image.texture +
		"', pos=(" + std::to_string(image.x) + "," + std::to_string(image.y) + ")" +
		", size=(" + std::to_string(image.width) + "x" + std::to_string(image.height) + ")");
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
		json << "      \"parent\": \"" << widget.parentName << "\",\n";
		json << "      \"skin\": \"" << widget.skin << "\",\n";
		json << "      \"text\": \"" << widget.text << "\",\n";
		json << "      \"texture\": \"" << widget.texture << "\",\n";
		json << "      \"layer\": \"" << widget.layer << "\",\n";
		json << "      \"x\": " << widget.x << ",\n";
		json << "      \"y\": " << widget.y << ",\n";
		json << "      \"width\": " << widget.width << ",\n";
		json << "      \"height\": " << widget.height << ",\n";
		json << "      \"visible\": " << (widget.visible ? "true" : "false") << ",\n";
		json << "      \"alpha\": " << widget.alpha << ",\n";
		json << "      \"action\": \"" << ActionToString(widget.action) << "\"\n";
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

	const std::string deletedWidgetName = asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)].name;
	asset.widgets.erase(asset.widgets.begin() + m_selectedWidgetIndex);
	for (GameGUIWidgetDef& widget : asset.widgets)
	{
		if (widget.parentName == deletedWidgetName)
		{
			widget.parentName.clear();
		}
	}
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
