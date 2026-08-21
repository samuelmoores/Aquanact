#include "Engine/UI/GameGUICreator.h"

#include <imgui.h>
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

	char textBuffer[256] = {};
	std::snprintf(textBuffer, sizeof(textBuffer), "%s", widget.text.c_str());
	if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer)))
	{
		widget.text = textBuffer;
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	// Treat the stored position as the image center in the editor.
	int imagePosition[2] = { widget.x + widget.width / 2, widget.y + widget.height / 2 };
	if (ImGui::DragInt2("Position", imagePosition, 1.0f))
	{
		widget.x = imagePosition[0] - widget.width / 2;
		widget.y = imagePosition[1] - widget.height / 2;
		SyncRuntimePreview();
	}

	if (ImGui::DragInt("Font Size", &widget.fontSize, 1, 1, 100))
	{
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

}
