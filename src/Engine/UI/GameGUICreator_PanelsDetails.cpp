#include "Engine/UI/GameGUICreator.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

void GameGUICreator::DrawWidgetDetails()
{
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
			const bool buttonControlledByPanel = widget.type == "Button" && std::any_of(asset.widgets.begin(), asset.widgets.end(), [&widget](const GameGUIWidgetDef& candidate)
			{
				return candidate.type == "Panel" && candidate.name == widget.parentName;
			});
			if (widget.type == "Button")
			{
				char buttonText[256] = {};
				std::snprintf(buttonText, sizeof(buttonText), "%s", widget.text.c_str());
				if (ImGui::InputText("Button text", buttonText, sizeof(buttonText)))
				{
					const std::string previousName = widget.name;
					widget.text = buttonText;
					widget.name = widget.text;
					for (GameGUIWidgetDef& otherWidget : asset.widgets)
					{
						if (otherWidget.parentName == previousName) otherWidget.parentName = widget.name;
					}
					SyncRuntimePreview();
				}
				if (ImGui::Checkbox("Show button image", &widget.useSkin))
				{
					SyncRuntimePreview();
				}
				if (!buttonControlledByPanel)
				{
					float textColour[3] = { 0.0f, 0.0f, 0.0f };
					std::sscanf(widget.textColor.c_str(), "%f %f %f", &textColour[0], &textColour[1], &textColour[2]);
					if (ImGui::ColorEdit3("Text color", textColour))
					{
						char colourValue[96] = {};
						std::snprintf(colourValue, sizeof(colourValue), "%.3f %.3f %.3f", textColour[0], textColour[1], textColour[2]);
						widget.textColor = colourValue;
						SyncRuntimePreview();
					}
				}
				const char* parentLabel = widget.parentName.empty() ? "<No Panel>" : widget.parentName.c_str();
				if (ImGui::BeginCombo("Parent panel", parentLabel))
				{
					if (ImGui::Selectable("<No Panel>", widget.parentName.empty())) widget.parentName.clear();
					for (GameGUIWidgetDef& candidate : asset.widgets)
					{
						if (candidate.type != "Panel") continue;
						const bool selected = widget.parentName == candidate.name;
						if (ImGui::Selectable(candidate.name.c_str(), selected))
						{
							widget.parentName = candidate.name;
							ApplyPanelButtonLayout(candidate);
						}
					}
					ImGui::EndCombo();
					SyncRuntimePreview();
				}
			}
			else if (widget.type == "Panel")
			{
				if (ImGui::Button("Add Button"))
				{
					m_newButtonParentPanel = widget.name;
					m_showCreateWidgetPopup = true;
					m_newWidgetIsImage = false;
					m_newWidgetIsProgressBar = false;
					m_newWidgetIsPanel = false;
					m_newWidgetAction = GameGUIActionType::None;
					m_newWidgetLaunchLevel.clear();
					m_newWidgetName[0] = '\0';
					m_newWidgetTexture[0] = '\0';
				}
				bool layoutChanged = ImGui::Checkbox("Show panel skin", &widget.useSkin);
				const char* panelSkins[] = { "PanelSkin", "WindowFrameSkin", "TabPanelSkin", "ClientDefaultSkin" };
				int skinIndex = 0;
				for (int i = 0; i < IM_ARRAYSIZE(panelSkins); ++i) if (widget.skin == panelSkins[i]) skinIndex = i;
				if (widget.useSkin && ImGui::Combo("Panel skin", &skinIndex, panelSkins, IM_ARRAYSIZE(panelSkins)))
				{
					widget.skin = panelSkins[skinIndex];
					layoutChanged = true;
				}
				layoutChanged |= ImGui::Checkbox("Uniform button spacing", &widget.uniformButtonSpacing);
				int buttonSize[2] = { widget.panelButtonWidth, widget.panelButtonHeight };
				if (ImGui::DragInt2("Button size", buttonSize, 1.0f, 1, 1000))
				{
					widget.panelButtonWidth = std::max(1, buttonSize[0]);
					widget.panelButtonHeight = std::max(1, buttonSize[1]);
					layoutChanged = true;
				}
				float panelTextColour[3] = { 0.0f, 0.0f, 0.0f };
				std::sscanf(widget.panelButtonTextColor.c_str(), "%f %f %f", &panelTextColour[0], &panelTextColour[1], &panelTextColour[2]);
				if (ImGui::ColorEdit3("Button text color", panelTextColour))
				{
					char colourValue[96] = {};
					std::snprintf(colourValue, sizeof(colourValue), "%.3f %.3f %.3f", panelTextColour[0], panelTextColour[1], panelTextColour[2]);
					widget.panelButtonTextColor = colourValue;
					layoutChanged = true;
				}
				if (widget.uniformButtonSpacing)
				{
					layoutChanged |= ImGui::Checkbox("Horizontal layout", &widget.horizontalButtonLayout);
					layoutChanged |= ImGui::SliderInt("Panel padding", &widget.panelPadding, 0, 100);
				}
				if (layoutChanged)
				{
					ApplyPanelButtonLayout(widget);
					SyncRuntimePreview();
				}
			}
			if (widget.type == "Button" && widget.action == GameGUIActionType::NewGame)
			{
				ImGui::Separator();
				ImGui::TextUnformatted("New Game Launch");
				SceneManager& sceneManager = Root::Current().Levels();
				const auto levelNames = sceneManager.SceneNames(SceneManager::SceneKind::Level);
				const char* launchLabel = widget.launchLevel.empty() ? "<Select Level>" : widget.launchLevel.c_str();
				if (ImGui::BeginCombo("Launch Level", launchLabel))
				{
					for (const std::string& levelName : levelNames)
					{
						const bool selected = widget.launchLevel == levelName;
						if (ImGui::Selectable(levelName.c_str(), selected))
						{
							widget.launchLevel = levelName;
							SyncRuntimePreview();
							SaveSelectedRoleGUI();
						}
						if (selected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				if (levelNames.empty())
				{
					ImGui::TextDisabled("No gameplay scenes are available.");
				}
			}
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
			if (buttonControlledByPanel)
			{
				ImGui::TextDisabled("Size and text color are controlled by parent panel.");
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
				if (widget.type == "Panel") ApplyPanelButtonLayout(widget);
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
				if (widget.type == "Panel") ApplyPanelButtonLayout(widget);
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

