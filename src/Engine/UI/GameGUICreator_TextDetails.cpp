#include "Engine/UI/GameGUICreator.h"

#include <imgui.h>
#include <cstdio>

void GameGUICreator::DrawTextWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	(void)asset;

	// Text widgets are the simplest editor case: rename and edit the caption.
	char nameBuffer[256] = {};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", widget.name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		widget.name = nameBuffer;
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	char textBuffer[256] = {};
	std::snprintf(textBuffer, sizeof(textBuffer), "%s", widget.text.c_str());
	if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer)))
	{
		widget.text = textBuffer;
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}
}
