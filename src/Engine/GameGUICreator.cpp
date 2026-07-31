#include "Engine/GameGUICreator.h"

#include "Engine/FrontEndManager.h"
#include "Engine/RenderManager.h"
#include "Engine/Debug.h"
#include "Engine/Window.h"
#include "Engine/Camera.h"
#include "Engine/Root.h"
#include "Engine/FileSystem.h"
#include "Engine/Level.h"
#include "Engine/LevelManager.h"

#include <MYGUI/MyGUI_Colour.h>
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>

namespace {
	int ReadIntField(const std::string& value, int fallback = 0)
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

	const char* GUIName(std::size_t index)
	{
		static constexpr const char* names[] = { "Main Menu", "HUD", "Pause Menu", "Player UI" };
		return index < std::size(names) ? names[index] : "Unknown";
	}

	const char* GUIAssetName(std::size_t index)
	{
		static constexpr const char* names[] = { "MainMenu", "HUD", "PauseMenu", "PlayerUI" };
		return index < std::size(names) ? names[index] : "Unknown";
	}

	const char* ActionLabel(GameGUIActionType action)
	{
		switch (action)
		{
		case GameGUIActionType::NewGame:
			return "New Game";
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
		default:
			return "None";
		}
	}

	MyGUI::Colour ParseColour(const std::string& value, const MyGUI::Colour& fallback)
	{
		try
		{
			if (value.empty())
			{
				return fallback;
			}
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

	Entity* FindEntity(Level* level, const std::string& name)
	{
		if (!level)
		{
			return nullptr;
		}

		for (const auto& entity : level->Entities())
		{
			if (entity && entity->Name() == name)
			{
				return entity.get();
			}
		}
		return nullptr;
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

	std::filesystem::path TextureDirectory()
	{
		return SourceRoot() / "assets" / "textures";
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
		const std::filesystem::path relativeToAssets = Root::Current().FileSystemRef().Relative(absolutePath, SourceRoot() / "assets", ec);
		if (!ec && !relativeToAssets.empty())
		{
			return relativeToAssets.generic_string();
		}
		return absolutePath.generic_string();
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
			widget.x = ReadIntField(readField("\"x\":", widgetPos));
			widget.y = ReadIntField(readField("\"y\":", widgetPos));
			widget.width = ReadIntField(readField("\"width\":", widgetPos), 100);
			widget.height = ReadIntField(readField("\"height\":", widgetPos), 30);
			widget.defaultTextureWidth = widget.textureWidth;
			widget.defaultTextureHeight = widget.textureHeight;
			widget.fontSize = ReadIntField(readField("\"fontSize\":", widgetPos), 0);
			widget.visible = readField("\"visible\":", widgetPos).find("true") != std::string::npos;
			widget.alpha = std::stof(readField("\"alpha\":", widgetPos));
			widget.highlightColor = readField("\"highlightColor\":", widgetPos);
			widget.clickedColor = readField("\"clickedColor\":", widgetPos);
			widget.action = StringToAction(readField("\"action\":", widgetPos));
			widget.bindEntity = readField("\"bindEntity\":", widgetPos);
			widget.bindComponent = readField("\"bindComponent\":", widgetPos);
			widget.bindMember = readField("\"bindMember\":", widgetPos);
			widget.bindEvent = readField("\"bindEvent\":", widgetPos);
			asset.widgets.push_back(widget);
			widgetPos = contents.find("\"type\": \"", widgetPos + 1);
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
	if (m_initialized)
	{
		return;
	}

	m_window = &window;
	m_assets.clear();
	m_assets.resize(GUIIndex(GUIRole::Count));
	for (std::size_t i = 0; i < m_assets.size(); ++i)
	{
		const std::filesystem::path assetPath = AssetDirectory() / (std::string(GUIAssetName(i)) + ".json");
		if (std::filesystem::exists(assetPath))
		{
			m_assets[i] = LoadAssetFile(assetPath);
		}
		else
		{
			m_assets[i] = MakeEmptyAsset(GUIAssetName(i));
		}
	}
	m_selectedGUI = GUIRole::MainMenu;
	m_selectedWidgetIndex = -1;
	m_newWidgetName[0] = '\0';
	m_newWidgetText[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	m_newWidgetIsText = false;
	m_newWidgetIsImage = false;
	m_lockWidgetSize = false;
	m_newWidgetAction = GameGUIActionType::None;
	m_showTexturePickerPopup = false;
	m_showBindingPopup = false;
	m_pendingProgressBarCreation = false;
	m_pendingProgressBarBindingComplete = false;
	m_texturePickerTarget = TexturePickerTarget::None;
	m_texturePickerRootDirectory.clear();
	m_texturePickerCurrentDirectory.clear();
	m_texturePickerSelectedPath.clear();
	m_bindingWidgetName.clear();
	m_lockedWidgetSizeRatio = 1.0f;
	m_previousShowAxis = true;
	m_previousShowGrid = true;
	m_previousViewStateCaptured = false;
	m_initialized = true;
	m_selectedWidgetIndex = m_assets[GUIIndex(m_selectedGUI)].widgets.empty() ? -1 : 0;
	SyncRuntimePreview();
}

void GameGUICreator::shutDown()
{
	m_window = nullptr;
	m_initialized = false;
	m_newWidgetName[0] = '\0';
	m_newWidgetText[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	m_newWidgetIsText = false;
	m_newWidgetIsImage = false;
	m_lockWidgetSize = false;
	m_showTexturePickerPopup = false;
	m_pendingProgressBarCreation = false;
	m_pendingProgressBarBindingComplete = false;
	m_newWidgetAction = GameGUIActionType::None;
	m_texturePickerTarget = TexturePickerTarget::None;
	m_texturePickerRootDirectory.clear();
	m_texturePickerCurrentDirectory.clear();
	m_texturePickerSelectedPath.clear();
	m_showBindingPopup = false;
	m_bindingWidgetName.clear();
	m_lockedWidgetSizeRatio = 1.0f;
	RestoreEditorViewState();
	m_assets.clear();
	m_selectedGUI = GUIRole::MainMenu;
	m_selectedWidgetIndex = -1;
}

void GameGUICreator::BeginFrame()
{
}

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
	return AssetDirectory() / (asset.name + ".json");
}

std::size_t GameGUICreator::GUIIndex(GUIRole role)
{
	return static_cast<std::size_t>(role);
}

const char* GameGUICreator::GUIName(GUIRole role)
{
	return ::GUIName(GUIIndex(role));
}

void GameGUICreator::Draw(const Camera&)
{
	if (!m_initialized)
	{
		return;
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("UI"))
		{
			if (ImGui::MenuItem("Leave Creator"))
			{
				Root::Current().FrontEnd().ReturnToEngineGUIEditor();
				Root::Current().Render().SetActiveCamera(Root::Current().Render().GetEngineCamera());
				Root::Current().Debugger().LogMessage("Leave Creator requested");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Current GUI"))
			{
				SaveSelectedRoleGUI();
				Root::Current().Debugger().LogMessage("Save Current GUI requested");
			}
			if (ImGui::MenuItem("Load Current GUI"))
			{
				LoadSelectedRoleGUI();
				SyncRuntimePreview();
				Root::Current().Debugger().LogMessage("Load Current GUI requested");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Create Button"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsText = false;
				m_newWidgetIsImage = false;
				m_newWidgetIsProgressBar = false;
				m_newWidgetName[0] = '\0';
				m_newWidgetText[0] = '\0';
				m_newWidgetTexture[0] = '\0';
			}
			if (ImGui::MenuItem("Create Text"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsText = true;
				m_newWidgetIsImage = false;
				m_newWidgetIsProgressBar = false;
				m_newWidgetName[0] = '\0';
				std::snprintf(m_newWidgetText, sizeof(m_newWidgetText), "New Text");
				m_newWidgetTexture[0] = '\0';
			}
			if (ImGui::MenuItem("Create Image"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsText = false;
				m_newWidgetIsImage = true;
				m_newWidgetIsProgressBar = false;
				m_newWidgetName[0] = '\0';
				m_newWidgetText[0] = '\0';
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "textures/example.png");
			}
			if (ImGui::MenuItem("Create Progress Bar"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsText = false;
				m_newWidgetIsImage = false;
				m_newWidgetIsProgressBar = true;
				m_newWidgetName[0] = '\0';
				m_newWidgetText[0] = '\0';
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "textures/example.png");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	DrawCreateWidgetPopup();
	DrawBindingPopup();
	DrawTexturePickerPopup();

	ImGui::Begin("GameGUIs");
	for (std::size_t i = 0; i < m_assets.size(); ++i)
	{
		const bool selected = GUIIndex(m_selectedGUI) == i;
		if (ImGui::Selectable(GUIName(static_cast<GUIRole>(i)), selected))
		{
			m_selectedGUI = static_cast<GUIRole>(i);
			m_selectedWidgetIndex = m_assets[i].widgets.empty() ? -1 : 0;
			SyncRuntimePreview();
		}
	}
	ImGui::End();

	ImGui::Begin("Widget List");
	if (!CurrentRoleGUI().widgets.empty())
	{
		GameGUIAsset& asset = CurrentRoleGUI();
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
	}
	else
	{
		ImGui::TextUnformatted("No widgets.");
	}
	ImGui::End();

	ImGui::Begin("Widget Details");
	if (CurrentRoleGUI().widgets.empty())
	{
		ImGui::TextUnformatted("No widget selected.");
	}
	else
	{
		GameGUIAsset& asset = CurrentRoleGUI();
		if (m_selectedWidgetIndex < 0 || m_selectedWidgetIndex >= static_cast<int>(asset.widgets.size()))
		{
			ImGui::TextUnformatted("No widget selected.");
		}
		else
		{
			GameGUIWidgetDef& widget = asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)];
			ImGui::Text("Name: %s", widget.name.c_str());
			ImGui::Text("Type: %s", widget.type.c_str());
			int position[2] = { widget.x, widget.y };
			if (ImGui::DragInt2("Position", position, 1.0f))
			{
				widget.x = position[0];
				widget.y = position[1];
				SyncRuntimePreview();
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset##Position"))
			{
				widget.x = 0;
				widget.y = 0;
				SyncRuntimePreview();
			}
			if (widget.type == "TextBox" || widget.type == "Text")
			{
				int fontSize = widget.fontSize;
				if (ImGui::DragInt("Font Size", &fontSize, 1.0f, 0, 200))
				{
					widget.fontSize = std::max(0, fontSize);
					SyncRuntimePreview();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset##FontSize"))
				{
					widget.fontSize = 0;
					SyncRuntimePreview();
				}
			}
			else
			{
				int size[2] = { widget.width, widget.height };
				if (ImGui::Checkbox("Lock Size", &m_lockWidgetSize) && m_lockWidgetSize)
				{
					m_lockedWidgetSizeRatio = widget.height > 0 ? static_cast<float>(widget.width) / static_cast<float>(widget.height) : 1.0f;
				}
				if (ImGui::DragInt2("Size", size, 1.0f))
				{
					widget.width = std::max(1, size[0]);
					widget.height = std::max(1, size[1]);
					if (m_lockWidgetSize)
					{
						widget.height = std::max(1, static_cast<int>(std::lround(static_cast<float>(widget.width) / m_lockedWidgetSizeRatio)));
						size[1] = widget.height;
					}
					SyncRuntimePreview();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset##Size"))
				{
					if (widget.type == "ProgressBar")
					{
						widget.width = widget.textureWidth;
						widget.height = widget.defaultTextureHeight;
					}
					else
					{
						widget.width = 100;
						widget.height = 30;
					}
					SyncRuntimePreview();
				}
			}

			if (widget.type == "ImageBox" || widget.type == "Image" || widget.type == "ProgressBar")
			{
				const char* textureLabel = widget.texture.empty() ? "<No Texture>" : widget.texture.c_str();
				ImGui::Text("Texture: %s", textureLabel);
				if (ImGui::Button("Browse...##SelectedWidgetTexture"))
				{
					OpenTexturePicker(TexturePickerTarget::SelectedWidgetTexture);
				}
			}

			ImGui::Separator();
			ImGui::TextUnformatted("Bindings");
			if (ImGui::Button(widget.type == "ProgressBar" ? "Edit Binding" : "Add Binding"))
			{
				m_bindingWidgetName = widget.name;
				m_showBindingPopup = true;
			}
			if (!widget.bindEntity.empty() || !widget.bindComponent.empty() || !widget.bindMember.empty() || !widget.bindEvent.empty())
			{
				ImGui::Text("Entity: %s", widget.bindEntity.empty() ? "<None>" : widget.bindEntity.c_str());
				ImGui::Text("Component: %s", widget.bindComponent.empty() ? "<None>" : widget.bindComponent.c_str());
				if (widget.type == "ProgressBar")
				{
					ImGui::Text("Value: %s", widget.bindMember.empty() ? "<None>" : widget.bindMember.c_str());
				}
				else
				{
					ImGui::Text("Event: %s", widget.bindEvent.empty() ? "<None>" : widget.bindEvent.c_str());
				}
				if (ImGui::Button("Clear Binding"))
				{
					widget.bindEntity.clear();
					widget.bindComponent.clear();
					widget.bindMember.clear();
					widget.bindEvent.clear();
					SyncRuntimePreview();
				}
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

void GameGUICreator::DrawCreateWidgetPopup()
{
	if (m_showCreateWidgetPopup)
	{
		ImGui::OpenPopup("Create Widget");
		m_showCreateWidgetPopup = false;
	}

	if (ImGui::BeginPopupModal("Create Widget", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const char* widgetKind = m_newWidgetIsProgressBar ? "Create a progress bar widget:" : (m_newWidgetIsImage ? "Create an image widget:" : (m_newWidgetIsText ? "Create a text widget:" : "Create a button widget:"));
		ImGui::TextUnformatted(widgetKind);
		ImGui::InputText("Name", m_newWidgetName, sizeof(m_newWidgetName));
		if (m_newWidgetIsText)
		{
			ImGui::InputText("Text", m_newWidgetText, sizeof(m_newWidgetText));
		}
		else if (m_newWidgetIsImage || m_newWidgetIsProgressBar)
		{
			ImGui::InputText("Texture", m_newWidgetTexture, sizeof(m_newWidgetTexture));
			ImGui::SameLine();
			if (ImGui::Button("Browse...##NewWidgetTexture"))
			{
				OpenTexturePicker(TexturePickerTarget::NewWidgetTexture);
			}
		}
		if (m_newWidgetIsProgressBar)
		{
			Level* activeLevel = Root::Current().Levels().ActiveLevel();
			ImGui::Separator();
			ImGui::TextUnformatted("Binding");

			GameGUIWidgetDef& preview = m_pendingProgressBarWidget;
			if (preview.name.empty())
			{
				preview.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "progress";
			}
			preview.texture = m_newWidgetTexture[0] != '\0' ? m_newWidgetTexture : "textures/example.png";
			preview.type = "ProgressBar";
			preview.layer = "Main";
			preview.width = 256;
			preview.height = 32;
			preview.textureWidth = 256;
			preview.textureHeight = 32;
			preview.defaultTextureWidth = 256;
			preview.defaultTextureHeight = 32;

			const char* entityLabel = preview.bindEntity.empty() ? "<Select Entity>" : preview.bindEntity.c_str();
			if (ImGui::BeginCombo("Entity", entityLabel))
			{
				if (activeLevel)
				{
					for (const auto& entity : activeLevel->Entities())
					{
						if (!entity)
						{
							continue;
						}

						const bool selected = preview.bindEntity == entity->Name();
						if (ImGui::Selectable(entity->Name().c_str(), selected))
						{
							preview.bindEntity = entity->Name();
							preview.bindComponent.clear();
							preview.bindMember.clear();
							preview.bindEvent.clear();
						}
						if (selected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
				}
				ImGui::EndCombo();
			}

			Entity* boundEntity = activeLevel ? FindEntity(activeLevel, preview.bindEntity) : nullptr;
			ImGui::BeginDisabled(!boundEntity);
			const char* componentLabel = preview.bindComponent.empty() ? "<Select Component>" : preview.bindComponent.c_str();
			if (ImGui::BeginCombo("Component", componentLabel))
			{
				for (Component* component : boundEntity ? boundEntity->Components() : std::vector<Component*>{})
				{
					if (!component || component->GetBindableMembers().empty())
					{
						continue;
					}

					const bool selected = preview.bindComponent == component->Name();
					if (ImGui::Selectable(component->Name(), selected))
					{
						preview.bindComponent = component->Name();
						preview.bindMember.clear();
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::EndDisabled();

			Component* boundComponent = boundEntity ? boundEntity->GetComponentByName(preview.bindComponent) : nullptr;
			ImGui::BeginDisabled(!boundComponent);
			const char* memberLabel = preview.bindMember.empty() ? "<Select Value>" : preview.bindMember.c_str();
			std::vector<BindableMember> bindableMembers = boundComponent ? boundComponent->GetBindableMembers() : std::vector<BindableMember>{};
			if (ImGui::BeginCombo("Value", memberLabel))
			{
				for (const BindableMember& member : bindableMembers)
				{
					if (member.typeName != "int" && member.typeName != "float")
					{
						continue;
					}

					const bool selected = preview.bindMember == member.name;
					const char* label = member.displayName.empty() ? member.name.c_str() : member.displayName.c_str();
					if (ImGui::Selectable(label, selected))
					{
						preview.bindMember = member.name;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::EndDisabled();
		}
		else
		{
			GameGUIActionType action = m_newWidgetAction;
			if (ImGui::BeginCombo("Action", ActionLabel(action)))
			{
				const GameGUIActionType options[] = { GameGUIActionType::None, GameGUIActionType::NewGame };
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
				else if (m_newWidgetIsProgressBar)
				{
					if (m_pendingProgressBarWidget.name.empty())
					{
						m_pendingProgressBarWidget.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "progress";
					}
					AddProgressBarWidget();
					m_pendingProgressBarWidget = {};
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
			Root::Current().Debugger().LogMessage(m_newWidgetIsProgressBar ? "Create Progress Bar requested" : (m_newWidgetIsImage ? "Create Image requested" : (m_newWidgetIsText ? "Create Text requested" : "Create Button requested")));
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

void GameGUICreator::DrawBindingPopup()
{
	if (m_showBindingPopup)
	{
		ImGui::OpenPopup("Add Binding");
		m_showBindingPopup = false;
	}

	if (!ImGui::BeginPopupModal("Add Binding", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

		GameGUIAsset& asset = CurrentRoleGUI();
	GameGUIWidgetDef* widget = nullptr;
	GameGUIWidgetDef* draft = m_pendingProgressBarCreation ? &m_pendingProgressBarWidget : nullptr;
	if (draft)
	{
		widget = draft;
	}
	else
	{
		for (GameGUIWidgetDef& candidate : asset.widgets)
		{
			if (candidate.name == m_bindingWidgetName)
			{
				widget = &candidate;
				break;
			}
		}
	}

	if (!widget)
	{
		ImGui::TextDisabled("Binding target not found.");
		if (ImGui::Button("Close"))
		{
			m_bindingWidgetName.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return;
	}

	ImGui::TextUnformatted("Binding target");
	ImGui::Text("Name: %s", widget->name.empty() ? "<Unnamed>" : widget->name.c_str());
	ImGui::Text("Type: %s", widget->type.empty() ? "<Unknown>" : widget->type.c_str());
	if (widget->type == "ProgressBar")
	{
		ImGui::Text("Texture: %s", widget->texture.empty() ? "<No Texture>" : widget->texture.c_str());
		const ImVec2 previewSize(180.0f, 24.0f);
		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 background = IM_COL32(45, 45, 50, 255);
		const ImU32 fill = IM_COL32(90, 160, 255, 255);
		drawList->AddRectFilled(cursor, ImVec2(cursor.x + previewSize.x, cursor.y + previewSize.y), background, 4.0f);
		const float fillWidth = previewSize.x * 0.65f;
		drawList->AddRectFilled(cursor, ImVec2(cursor.x + fillWidth, cursor.y + previewSize.y), fill, 4.0f);
		drawList->AddRect(cursor, ImVec2(cursor.x + previewSize.x, cursor.y + previewSize.y), IM_COL32(0, 0, 0, 255), 4.0f, 0, 1.0f);
		ImGui::Dummy(ImVec2(previewSize.x, previewSize.y));
	}
	ImGui::Separator();

	Level* activeLevel = Root::Current().Levels().ActiveLevel();
	if (!activeLevel)
	{
		ImGui::TextDisabled("No active level is available.");
		if (ImGui::Button("Close"))
		{
			m_bindingWidgetName.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return;
	}

	const char* entityLabel = widget->bindEntity.empty() ? "<Select Entity>" : widget->bindEntity.c_str();
	if (ImGui::BeginCombo("Entity", entityLabel))
	{
		for (const auto& entity : activeLevel->Entities())
		{
			if (!entity)
			{
				continue;
			}

			const bool selected = widget->bindEntity == entity->Name();
			if (ImGui::Selectable(entity->Name().c_str(), selected))
			{
				widget->bindEntity = entity->Name();
				widget->bindComponent.clear();
				widget->bindMember.clear();
				widget->bindEvent.clear();
				SyncRuntimePreview();
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	Entity* boundEntity = FindEntity(activeLevel, widget->bindEntity);
	ImGui::BeginDisabled(!boundEntity);
	const char* componentLabel = widget->bindComponent.empty() ? "<Select Component>" : widget->bindComponent.c_str();
	if (ImGui::BeginCombo("Component", componentLabel))
	{
		for (Component* component : boundEntity ? boundEntity->Components() : std::vector<Component*>{})
		{
			if (!component)
			{
				continue;
			}

			if (widget->type == "ProgressBar")
			{
				if (component->GetBindableMembers().empty())
				{
					continue;
				}
			}
			else if (component->GetBindableEvents().empty())
			{
				continue;
			}

			const bool selected = widget->bindComponent == component->Name();
			if (ImGui::Selectable(component->Name(), selected))
			{
				widget->bindComponent = component->Name();
				widget->bindMember.clear();
				widget->bindEvent.clear();
				SyncRuntimePreview();
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::EndDisabled();

	Component* boundComponent = boundEntity ? boundEntity->GetComponentByName(widget->bindComponent) : nullptr;
	if (widget->type == "ProgressBar")
	{
		ImGui::BeginDisabled(!boundComponent);
		const char* memberLabel = widget->bindMember.empty() ? "<Select Member>" : widget->bindMember.c_str();
		std::vector<BindableMember> bindableMembers = boundComponent ? boundComponent->GetBindableMembers() : std::vector<BindableMember>{};
		const auto selectedMember = std::find_if(bindableMembers.begin(), bindableMembers.end(), [&widget](const BindableMember& member)
		{
			return member.name == widget->bindMember;
		});
		if (selectedMember != bindableMembers.end() && !selectedMember->displayName.empty())
		{
			memberLabel = selectedMember->displayName.c_str();
		}
		if (ImGui::BeginCombo("Member", memberLabel))
		{
			for (const BindableMember& member : bindableMembers)
			{
				if (member.typeName != "int" && member.typeName != "float")
				{
					continue;
				}

				const bool selected = widget->bindMember == member.name;
				const char* label = member.displayName.empty() ? member.name.c_str() : member.displayName.c_str();
				if (ImGui::Selectable(label, selected))
				{
					widget->bindMember = member.name;
					SyncRuntimePreview();
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}
	else
	{
		ImGui::BeginDisabled(!boundComponent);
		const char* eventLabel = widget->bindEvent.empty() ? "<Select Event>" : widget->bindEvent.c_str();
		std::vector<BindableEvent> bindableEvents = boundComponent ? boundComponent->GetBindableEvents() : std::vector<BindableEvent>{};
		const auto selectedEvent = std::find_if(bindableEvents.begin(), bindableEvents.end(), [&widget](const BindableEvent& event)
		{
			return event.name == widget->bindEvent;
		});
		if (selectedEvent != bindableEvents.end() && !selectedEvent->displayName.empty())
		{
			eventLabel = selectedEvent->displayName.c_str();
		}
		if (ImGui::BeginCombo("Event", eventLabel))
		{
			for (const BindableEvent& event : bindableEvents)
			{
				const bool selected = widget->bindEvent == event.name;
				const char* label = event.displayName.empty() ? event.name.c_str() : event.displayName.c_str();
				if (ImGui::Selectable(label, selected))
				{
					widget->bindEvent = event.name;
					SyncRuntimePreview();
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}

	if (ImGui::Button("Close"))
	{
		if (m_pendingProgressBarCreation)
		{
			m_pendingProgressBarBindingComplete = true;
			m_showTexturePickerPopup = true;
		}
		m_bindingWidgetName.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void GameGUICreator::DrawTexturePickerPopup()
{
	if (m_showTexturePickerPopup)
	{
		ImGui::OpenPopup("Select Texture");
		m_showTexturePickerPopup = false;
	}

	if (!ImGui::BeginPopupModal("Select Texture", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	if (m_texturePickerRootDirectory.empty())
	{
		m_texturePickerRootDirectory = TextureDirectory();
		m_texturePickerCurrentDirectory = m_texturePickerRootDirectory;
	}

	ImGui::Text("Root: %s", m_texturePickerRootDirectory.generic_string().c_str());
	ImGui::Text("Current: %s", m_texturePickerCurrentDirectory.generic_string().c_str());
	const bool canGoUp = !m_texturePickerCurrentDirectory.empty() && m_texturePickerCurrentDirectory != m_texturePickerRootDirectory;
	if (ImGui::Button("Up") && canGoUp)
	{
		m_texturePickerCurrentDirectory = m_texturePickerCurrentDirectory.parent_path();
		m_texturePickerSelectedPath.clear();
	}

	ImGui::BeginChild("TextureFileExplorer", ImVec2(640.0f, 320.0f), true);
	std::error_code ec;
	if (!std::filesystem::exists(m_texturePickerCurrentDirectory, ec) || ec)
	{
		ImGui::TextDisabled("Texture directory does not exist.");
	}
	else
	{
		std::vector<std::filesystem::directory_entry> directories;
		std::vector<std::filesystem::directory_entry> files;
		for (const auto& entry : std::filesystem::directory_iterator(m_texturePickerCurrentDirectory, ec))
		{
			if (ec)
			{
				break;
			}

			if (entry.is_directory())
			{
				directories.push_back(entry);
			}
			else if (entry.is_regular_file() && IsSupportedTextureFile(entry.path()))
			{
				files.push_back(entry);
			}
		}

		const auto sortEntries = [](std::vector<std::filesystem::directory_entry>& entries)
		{
			std::sort(entries.begin(), entries.end(), [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
			{
				return a.path().filename().string() < b.path().filename().string();
			});
		};
		sortEntries(directories);
		sortEntries(files);

		for (const auto& entry : directories)
		{
			const std::string label = "[Dir] " + entry.path().filename().string();
			if (ImGui::Selectable(label.c_str(), false))
			{
				m_texturePickerCurrentDirectory = entry.path();
				m_texturePickerSelectedPath.clear();
			}
		}

		for (const auto& entry : files)
		{
			const bool selected = m_texturePickerSelectedPath == entry.path();
			if (ImGui::Selectable(entry.path().filename().string().c_str(), selected))
			{
				m_texturePickerSelectedPath = entry.path();
			}
		}

		if (directories.empty() && files.empty())
		{
			ImGui::TextDisabled("No texture files found in this directory.");
		}
	}
	ImGui::EndChild();

	if (!m_texturePickerSelectedPath.empty())
	{
		ImGui::Text("Selected: %s", MakePortableTexturePath(m_texturePickerSelectedPath).c_str());
	}
	else
	{
		ImGui::TextDisabled("Selected: <None>");
	}

	const bool canSelect = !m_texturePickerSelectedPath.empty();
	if (ImGui::Button("Select") && canSelect)
	{
		const TexturePickerTarget target = m_texturePickerTarget;
		const std::string portablePath = MakePortableTexturePath(m_texturePickerSelectedPath);
		if (target == TexturePickerTarget::NewWidgetTexture)
		{
			if (m_pendingProgressBarCreation)
			{
				m_pendingProgressBarWidget.texture = portablePath;
				AddProgressBarWidget();
				m_pendingProgressBarCreation = false;
				m_pendingProgressBarBindingComplete = false;
				SyncRuntimePreview();
			}
			else
			{
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "%s", portablePath.c_str());
			}
		}
		else if (target == TexturePickerTarget::SelectedWidgetTexture &&
			true)
		{
			GameGUIAsset& asset = CurrentRoleGUI();
			if (m_selectedWidgetIndex >= 0 && m_selectedWidgetIndex < static_cast<int>(asset.widgets.size()))
			{
				asset.widgets[static_cast<std::size_t>(m_selectedWidgetIndex)].texture = portablePath;
				SyncRuntimePreview();
			}
		}

		m_texturePickerTarget = TexturePickerTarget::None;
		m_texturePickerSelectedPath.clear();
		if (target == TexturePickerTarget::NewWidgetTexture)
		{
			m_showCreateWidgetPopup = true;
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		const TexturePickerTarget target = m_texturePickerTarget;
		m_texturePickerTarget = TexturePickerTarget::None;
		m_texturePickerSelectedPath.clear();
		if (target == TexturePickerTarget::NewWidgetTexture)
		{
			m_showCreateWidgetPopup = true;
		}
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void GameGUICreator::OpenTexturePicker(TexturePickerTarget target)
{
	m_texturePickerTarget = target;
	m_texturePickerRootDirectory = TextureDirectory();
	m_texturePickerCurrentDirectory = m_texturePickerRootDirectory;
	m_texturePickerSelectedPath.clear();
	m_showTexturePickerPopup = true;
}

void GameGUICreator::AddButtonWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		Root::Current().Debugger().LogMessage("Add button widget skipped: no active GUI");
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

		GameGUIAsset& asset = CurrentRoleGUI();
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
	Root::Current().Debugger().LogMessage(
		std::string("Created button widget in ") + GUIName(m_selectedGUI) + ": name='" + button.name +
		"', gui='" + asset.name +
		"', pos=(" + std::to_string(button.x) + "," + std::to_string(button.y) + ")" +
		", size=(" + std::to_string(button.width) + "x" + std::to_string(button.height) + ")");
}

void GameGUICreator::AddTextWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		Root::Current().Debugger().LogMessage("Add text widget skipped: no active GUI");
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
	text.fontSize = 0;
	text.action = GameGUIActionType::None;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	text.x = std::max(0, (framebufferWidth - text.width) / 2);
	text.y = std::max(0, (framebufferHeight - text.height) / 2);

		GameGUIAsset& asset = CurrentRoleGUI();
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
	Root::Current().Debugger().LogMessage(
		std::string("Created text widget in ") + GUIName(m_selectedGUI) + ": name='" + text.name +
		"', gui='" + asset.name +
		"', pos=(" + std::to_string(text.x) + "," + std::to_string(text.y) + ")" +
		", size=(" + std::to_string(text.width) + "x" + std::to_string(text.height) + ")");
}

void GameGUICreator::AddImageWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		Root::Current().Debugger().LogMessage("Add image widget skipped: no active GUI");
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
	image.fontSize = 0;
	image.action = GameGUIActionType::None;

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	image.x = std::max(0, (framebufferWidth - image.width) / 2);
	image.y = std::max(0, (framebufferHeight - image.height) / 2);

		GameGUIAsset& asset = CurrentRoleGUI();
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
	Root::Current().Debugger().LogMessage(
		std::string("Created image widget in ") + GUIName(m_selectedGUI) + ": name='" + image.name +
		"', gui='" + asset.name +
		"', texture='" + image.texture +
		"', pos=(" + std::to_string(image.x) + "," + std::to_string(image.y) + ")" +
		", size=(" + std::to_string(image.width) + "x" + std::to_string(image.height) + ")");
}

void GameGUICreator::AddProgressBarWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		Root::Current().Debugger().LogMessage("Add progress bar widget skipped: no active GUI");
		return;
	}

	GameGUIWidgetDef progress;
	if (m_pendingProgressBarCreation)
	{
		progress = m_pendingProgressBarWidget;
	}
	else
	{
		progress.type = "ProgressBar";
		progress.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "progress";
		progress.skin = "ImageBox";
		progress.texture = m_newWidgetTexture[0] != '\0' ? m_newWidgetTexture : "textures/example.png";
		progress.layer = "Main";
		progress.width = 256;
		progress.height = 32;
		progress.textureWidth = progress.width;
		progress.textureHeight = progress.height;
		progress.defaultTextureWidth = progress.textureWidth;
		progress.defaultTextureHeight = progress.textureHeight;
		progress.fontSize = 0;
		progress.action = GameGUIActionType::None;
	}

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	if (m_window)
	{
		m_window->GetFramebufferSize(framebufferWidth, framebufferHeight);
	}
	progress.x = std::max(0, (framebufferWidth - progress.width) / 2);
	progress.y = std::max(0, (framebufferHeight - progress.height) / 2);

		GameGUIAsset& asset = CurrentRoleGUI();
	int suffix = 1;
	while (std::any_of(asset.widgets.begin(), asset.widgets.end(), [&progress](const GameGUIWidgetDef& widget)
	{
		return widget.name == progress.name;
	}))
	{
		progress.name = "progress_" + std::to_string(++suffix);
	}

	asset.widgets.push_back(progress);
	m_selectedWidgetIndex = static_cast<int>(asset.widgets.size() - 1);
	m_newWidgetName[0] = '\0';
	m_newWidgetTexture[0] = '\0';
	m_pendingProgressBarWidget = {};
	Root::Current().Debugger().LogMessage(
		std::string("Created progress bar widget in ") + GUIName(m_selectedGUI) + ": name='" + progress.name +
		"', gui='" + asset.name +
		"', texture='" + progress.texture +
		"', pos=(" + std::to_string(progress.x) + "," + std::to_string(progress.y) + ")" +
		", size=(" + std::to_string(progress.width) + "x" + std::to_string(progress.height) + ")");
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

void GameGUICreator::DeleteSelectedWidget()
{
	if (GUIIndex(m_selectedGUI) >= m_assets.size())
	{
		return;
	}

	GameGUIAsset& asset = CurrentRoleGUI();
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



