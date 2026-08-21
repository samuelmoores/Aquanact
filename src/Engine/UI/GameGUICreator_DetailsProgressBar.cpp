#include "Engine/UI/GameGUICreatorView.h"
#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

void GameGUICreatorView::DrawProgressBarWidgetDetails(GameGUICreator& creator, GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	char nameBuffer[256] = {};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", widget.name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		if (creator.IsWidgetNameAvailable(asset, nameBuffer, &widget))
		{
			widget.name = nameBuffer;
			creator.SyncRuntimePreview();
			creator.SaveSelectedRoleGUI();
		}
	}
	creator.DrawWidgetParentPanelField(asset, widget);

	if (GameGUICreatorHelpers::DrawTextureCombo("Fill texture", widget.texture, false, "<No Texture>"))
	{
		if (GameGUICreatorHelpers::RefreshTextureBaseline(widget, widget.texture, true))
		{
			widget.width = widget.defaultWidth;
			widget.height = widget.defaultHeight;
		}
		creator.SyncRuntimePreview();
		creator.SaveSelectedRoleGUI();
	}

	int position[2] = { widget.x, widget.y };
	bool changed = false;
	if (ImGui::DragInt2("Position", position, 1.0f))
	{
		widget.x = position[0];
		widget.y = position[1];
		changed = true;
	}

	int size[2] = { widget.width, widget.height };
	if (ImGui::DragInt2("Size", size, 1.0f, 1, 4000))
	{
		widget.width = std::max(1, size[0]);
		widget.height = std::max(1, size[1]);
		changed = true;
	}

	if (changed)
	{
		creator.SyncRuntimePreview();
		creator.SaveSelectedRoleGUI();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Progress source");
	Scene* activeLevel = Root::Current().Scenes().ActiveLevel();
	if (GameGUICreatorHelpers::DrawProgressBindingControls(widget, activeLevel))
	{
		creator.SyncRuntimePreview();
		creator.SaveSelectedRoleGUI();
	}
	if (!widget.bindEntity.empty() || !widget.bindComponent.empty() || !widget.bindMember.empty())
	{
		if (ImGui::Button("Clear Binding"))
		{
			widget.bindEntity.clear();
			widget.bindComponent.clear();
			widget.bindMember.clear();
			creator.SyncRuntimePreview();
			creator.SaveSelectedRoleGUI();
		}
	}

}
