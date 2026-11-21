#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>

void GameGUICreator::DrawTextWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	// Text widgets are the simplest editor case: rename and edit the caption.
	char nameBuffer[256] = {};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", widget.name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		if (IsWidgetNameAvailable(asset, nameBuffer, &widget))
		{
			widget.name = nameBuffer;
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}
	}
	DrawWidgetParentPanelField(asset, widget);
	if (ImGui::Checkbox("World Anchored", &widget.worldAnchored))
	{
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
	if (widget.worldAnchored)
	{
		const char* anchorLabel = widget.worldAnchorEntity.empty() ? "<World Position>" : widget.worldAnchorEntity.c_str();
		if (ImGui::BeginCombo("Match Object", anchorLabel))
		{
			if (ImGui::Selectable("<World Position>", widget.worldAnchorEntity.empty()))
			{
				if (!widget.worldAnchorEntity.empty())
				{
					if (Scene* scene = Root::Current().Scenes().ActiveLevel())
					{
						for (const std::unique_ptr<Entity>& object : scene->Objects())
						{
							if (object && object->Name() == widget.worldAnchorEntity)
							{
								glm::vec3 position = object->WorldPosition();
								if (widget.worldAnchorUseSubmesh)
								{
									Mesh* mesh = object->GetMesh();
									if (mesh && widget.worldAnchorSubmeshIndex >= 0
										&& widget.worldAnchorSubmeshIndex < mesh->NumBuffers())
									{
										const glm::vec3 center = (mesh->SubMeshMinBounds(widget.worldAnchorSubmeshIndex)
											+ mesh->SubMeshMaxBounds(widget.worldAnchorSubmeshIndex)) * 0.5f;
										position = glm::vec3(object->BuildModelMatrix() * glm::vec4(center, 1.0f));
									}
								}
								position += glm::vec3(widget.worldX, widget.worldY, widget.worldZ);
								widget.worldX = position.x;
								widget.worldY = position.y;
								widget.worldZ = position.z;
								break;
							}
						}
					}
				}
				widget.worldAnchorEntity.clear();
				widget.worldAnchorUseSubmesh = false;
				SyncRuntimePreview();
				SaveSelectedRoleGUI();
			}
			if (Scene* scene = Root::Current().Scenes().ActiveLevel())
			{
				for (const std::unique_ptr<Entity>& object : scene->Objects())
				{
					if (object && ImGui::Selectable(object->Name().c_str(), widget.worldAnchorEntity == object->Name()))
					{
						widget.worldAnchorEntity = object->Name();
						widget.worldX = widget.worldY = widget.worldZ = 0.0f;
						widget.worldAnchorUseSubmesh = false;
						SyncRuntimePreview();
						SaveSelectedRoleGUI();
					}
				}
			}
			ImGui::EndCombo();
		}

		Entity* anchorObject = nullptr;
		if (!widget.worldAnchorEntity.empty())
		{
			if (Scene* scene = Root::Current().Scenes().ActiveLevel())
			{
				for (const std::unique_ptr<Entity>& object : scene->Objects())
				{
					if (object && object->Name() == widget.worldAnchorEntity)
					{
						anchorObject = object.get();
						break;
					}
				}
			}
		}
		Mesh* anchorMesh = anchorObject ? anchorObject->GetMesh() : nullptr;
		if (anchorMesh && anchorMesh->NumBuffers() > 0)
		{
			if (ImGui::Checkbox("Anchor To Submesh", &widget.worldAnchorUseSubmesh))
			{
				if (widget.worldAnchorUseSubmesh && widget.worldAnchorSubmeshIndex >= anchorMesh->NumBuffers())
					widget.worldAnchorSubmeshIndex = 0;
				SyncRuntimePreview();
				SaveSelectedRoleGUI();
			}
			if (widget.worldAnchorUseSubmesh)
			{
				widget.worldAnchorSubmeshIndex = std::clamp(widget.worldAnchorSubmeshIndex, 0, anchorMesh->NumBuffers() - 1);
				const std::string& submeshName = anchorMesh->SubMeshName(widget.worldAnchorSubmeshIndex);
				const std::string label = submeshName.empty() ? "Submesh " + std::to_string(widget.worldAnchorSubmeshIndex)
					: submeshName + " (Submesh " + std::to_string(widget.worldAnchorSubmeshIndex) + ")";
				if (ImGui::BeginCombo("Submesh", label.c_str()))
				{
					for (int index = 0; index < anchorMesh->NumBuffers(); ++index)
					{
						const std::string& name = anchorMesh->SubMeshName(index);
						const std::string item = name.empty() ? "Submesh " + std::to_string(index)
							: name + " (Submesh " + std::to_string(index) + ")";
						if (ImGui::Selectable((item + "##" + std::to_string(index)).c_str(), widget.worldAnchorSubmeshIndex == index))
						{
							widget.worldAnchorSubmeshIndex = index;
							SyncRuntimePreview();
							SaveSelectedRoleGUI();
						}
					}
					ImGui::EndCombo();
				}
			}
		}
		float worldPosition[3] = { widget.worldX, widget.worldY, widget.worldZ };
		if (ImGui::DragFloat3(widget.worldAnchorEntity.empty() ? "World Position" : "World Offset", worldPosition, 1.0f, 0.0f, 0.0f, "%.2f"))
		{
			widget.worldX = worldPosition[0];
			widget.worldY = worldPosition[1];
			widget.worldZ = worldPosition[2];
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}
	}

	char textBuffer[256] = {};
	std::snprintf(textBuffer, sizeof(textBuffer), "%s", widget.text.c_str());
	if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer)))
	{
		widget.text = textBuffer;
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
	float textColour[3] = { 0.0f, 0.0f, 0.0f };
	std::sscanf(widget.textColor.c_str(), "%f %f %f", &textColour[0], &textColour[1], &textColour[2]);
	if (ImGui::ColorEdit3("Text color", textColour))
	{
		char colourValue[96] = {};
		std::snprintf(colourValue, sizeof(colourValue), "%.3f %.3f %.3f",
			textColour[0], textColour[1], textColour[2]);
		widget.textColor = colourValue;
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
	int textBounds[2] = { widget.width, widget.height };
	if (ImGui::DragInt2("Widget size", textBounds, 1.0f, 1, 4096))
	{
		widget.width = std::max(1, textBounds[0]);
		widget.height = std::max(1, textBounds[1]);
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	// Treat the stored position as the image center in the editor.
	const char* positionLabel = widget.parentName.empty() ? "Screen Position" : "Position in Parent";
	int imagePosition[2] = {
		widget.parentName.empty() ? widget.x + widget.width / 2 : widget.x,
		widget.parentName.empty() ? widget.y + widget.height / 2 : widget.y };
	if (ImGui::DragInt2(positionLabel, imagePosition, 1.0f))
	{
		widget.x = widget.parentName.empty() ? imagePosition[0] - widget.width / 2 : imagePosition[0];
		widget.y = widget.parentName.empty() ? imagePosition[1] - widget.height / 2 : imagePosition[1];
		SyncRuntimePreview();
	}

	if (ImGui::DragInt("Text size (px)", &widget.fontSize, 1, 1, 256))
	{
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	ImGui::Separator();
	if (ImGui::Checkbox("Bindable", &widget.bindingEnabled))
	{
		if (!widget.bindingEnabled)
		{
			widget.bindEntity.clear();
			widget.bindComponent.clear();
			widget.bindMember.clear();
			widget.bindEvent.clear();
		}
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
	if (widget.bindingEnabled)
	{
		Scene* activeLevel = Root::Current().Scenes().ActiveLevel();
		if (GameGUICreatorHelpers::DrawValueBindingControls(widget, activeLevel))
		{
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}
		if (!widget.bindEntity.empty() || !widget.bindComponent.empty()
			|| !widget.bindMember.empty() || !widget.bindEvent.empty())
		{
			if (ImGui::Button("Clear Binding"))
			{
				widget.bindEntity.clear();
				widget.bindComponent.clear();
				widget.bindMember.clear();
				widget.bindEvent.clear();
				SyncRuntimePreview();
				SaveSelectedRoleGUI();
			}
		}
	}

}
