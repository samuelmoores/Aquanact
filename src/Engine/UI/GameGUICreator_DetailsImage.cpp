#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

void GameGUICreator::DrawImageWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	// Image widgets are intentionally simple: name plus one live texture picker.
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
		ImGui::TextDisabled("Projected into the screen from this world position; it does not use depth occlusion.");
		const char* anchorLabel = widget.worldAnchorEntity.empty()
			? "<World Position>" : widget.worldAnchorEntity.c_str();
		bool anchorChanged = false;
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
								glm::vec3 anchorPosition = object->WorldPosition();
								if (widget.worldAnchorUseSubmesh)
								{
									Mesh* mesh = object->GetMesh();
									if (mesh && widget.worldAnchorSubmeshIndex >= 0
										&& widget.worldAnchorSubmeshIndex < mesh->NumBuffers())
									{
										const glm::vec3 localCenter =
											(mesh->SubMeshMinBounds(widget.worldAnchorSubmeshIndex)
												+ mesh->SubMeshMaxBounds(widget.worldAnchorSubmeshIndex)) * 0.5f;
										anchorPosition = glm::vec3(object->BuildModelMatrix() * glm::vec4(localCenter, 1.0f));
									}
								}
								const glm::vec3 position = anchorPosition
									+ glm::vec3(widget.worldX, widget.worldY, widget.worldZ);
								widget.worldX = position.x;
								widget.worldY = position.y;
								widget.worldZ = position.z;
								widget.worldAnchorUseSubmesh = false;
								break;
							}
						}
					}
				}
				widget.worldAnchorEntity.clear();
				anchorChanged = true;
			}
			if (Scene* scene = Root::Current().Scenes().ActiveLevel())
			{
				for (const std::unique_ptr<Entity>& object : scene->Objects())
				{
					if (object && ImGui::Selectable(object->Name().c_str(),
						widget.worldAnchorEntity == object->Name()))
					{
						widget.worldAnchorEntity = object->Name();
						widget.worldX = widget.worldY = widget.worldZ = 0.0f;
						widget.worldAnchorUseSubmesh = false;
						anchorChanged = true;
					}
				}
			}
			ImGui::EndCombo();
		}
		if (anchorChanged)
		{
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
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
				if (widget.worldAnchorUseSubmesh
					&& widget.worldAnchorSubmeshIndex >= anchorMesh->NumBuffers())
					widget.worldAnchorSubmeshIndex = 0;
				SyncRuntimePreview();
				SaveSelectedRoleGUI();
			}
			if (widget.worldAnchorUseSubmesh)
			{
				if (widget.worldAnchorSubmeshIndex >= anchorMesh->NumBuffers())
					widget.worldAnchorSubmeshIndex = 0;
				const std::string& submeshName = anchorMesh->SubMeshName(widget.worldAnchorSubmeshIndex);
				const std::string selectedSubmeshLabel = submeshName.empty()
					? "Submesh " + std::to_string(widget.worldAnchorSubmeshIndex)
					: submeshName + " (Submesh " + std::to_string(widget.worldAnchorSubmeshIndex) + ")";
				if (ImGui::BeginCombo("Submesh", selectedSubmeshLabel.c_str()))
				{
					for (int index = 0; index < anchorMesh->NumBuffers(); ++index)
					{
						const std::string& name = anchorMesh->SubMeshName(index);
						const std::string label = name.empty()
							? "Submesh " + std::to_string(index)
							: name + " (Submesh " + std::to_string(index) + ")";
						if (ImGui::Selectable((label + "##" + std::to_string(index)).c_str(),
							widget.worldAnchorSubmeshIndex == index))
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
		else if (!widget.worldAnchorEntity.empty())
		{
			ImGui::TextDisabled("Submesh anchoring requires a mesh object in the active scene.");
		}

		float worldPosition[3] = { widget.worldX, widget.worldY, widget.worldZ };
		const char* worldPositionLabel = widget.worldAnchorEntity.empty() ? "World Position" : "World Offset";
		if (ImGui::DragFloat3(worldPositionLabel, worldPosition, 1.0f, 0.0f, 0.0f, "%.2f"))
		{
			widget.worldX = worldPosition[0];
			widget.worldY = worldPosition[1];
			widget.worldZ = worldPosition[2];
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}
	}

	if (ImGui::Checkbox("Solid White Box", &widget.solidWhiteBox))
	{
		if (widget.solidWhiteBox)
		{
			widget.texture.clear();
			if (widget.width <= 0 || widget.height <= 0)
				widget.width = widget.height = 128;
			widget.defaultWidth = widget.width;
			widget.defaultHeight = widget.height;
		}
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	if (widget.solidWhiteBox)
	{
		int size[2] = { widget.width, widget.height };
		if (ImGui::DragInt2("Size", size, 1.0f, 1, 8192))
		{
			widget.width = widget.defaultWidth = std::max(1, size[0]);
			widget.height = widget.defaultHeight = std::max(1, size[1]);
			SyncRuntimePreview();
			SaveSelectedRoleGUI();
		}
	}
	else
	{
		std::string selectedTexture = widget.texture;
		if (GameGUICreatorHelpers::DrawTextureCombo("Texture", selectedTexture, true, "<No Texture>"))
		{
			int textureWidth = 0;
			int textureHeight = 0;
			const bool validSelection = selectedTexture.empty()
				|| GameGUICreatorHelpers::GetTextureDimensions(
					GameGUICreatorHelpers::ResolveTexturePath(selectedTexture), textureWidth, textureHeight);
			if (!validSelection)
			{
				ImGui::TextDisabled("That image could not be opened. The previous selection was kept.");
			}
			else
			{
				widget.texture = selectedTexture;
				if (!selectedTexture.empty())
				{
					const int previousWidth = widget.width;
					const int previousHeight = widget.height;
					GameGUICreatorHelpers::RefreshTextureBaseline(widget, selectedTexture, false);
					widget.width = widget.defaultWidth;
					widget.height = widget.defaultHeight;
					widget.x += (previousWidth - widget.width) / 2;
					widget.y += (previousHeight - widget.height) / 2;
				}
				SyncRuntimePreview();
				SaveSelectedRoleGUI();
			}
		}
	}

	// Screen position is only used while the image is not anchored in the world.
	if (!widget.worldAnchored)
	{
		int imagePosition[2] = { widget.x + widget.width / 2, widget.y + widget.height / 2 };
		if (ImGui::DragInt2("Position", imagePosition, 1.0f))
		{
			widget.x = imagePosition[0] - widget.width / 2;
			widget.y = imagePosition[1] - widget.height / 2;
			SyncRuntimePreview();
		}
	}

	// Image widgets use a single scalar size so width and height stay in sync.
	const int baseWidth = widget.defaultWidth > 0 ? widget.defaultWidth : std::max(1, widget.width);
	const int baseHeight = widget.defaultHeight > 0 ? widget.defaultHeight : std::max(1, widget.height);
	float imageScale = static_cast<float>(widget.width) / static_cast<float>(baseWidth);
	if (ImGui::DragFloat("Size", &imageScale, 0.01f, 0.1f, 10.0f))
	{
		const int previousWidth = widget.width;
		const int previousHeight = widget.height;
		const int centerX = widget.x + previousWidth / 2;
		const int centerY = widget.y + previousHeight / 2;
		widget.width = std::max(1, static_cast<int>(std::lround(static_cast<float>(baseWidth) * imageScale)));
		widget.height = std::max(1, static_cast<int>(std::lround(static_cast<float>(baseHeight) * imageScale)));
		widget.x = centerX - widget.width / 2;
		widget.y = centerY - widget.height / 2;

		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

}
