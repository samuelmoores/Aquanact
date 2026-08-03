#include "Engine/UI/GameGUICreator.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

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

