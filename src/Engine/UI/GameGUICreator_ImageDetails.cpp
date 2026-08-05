#include "Engine/UI/GameGUICreator.h"
#include "Engine/UI/GameGUICreatorHelpers.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

void GameGUICreator::DrawImageWidgetDetails(GameGUIAsset& asset, GameGUIWidgetDef& widget)
{
	(void)asset;

	// Image widgets are intentionally simple: name plus one live texture picker.
	char nameBuffer[256] = {};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", widget.name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		widget.name = nameBuffer;
		SyncRuntimePreview();
		SaveSelectedRoleGUI();
	}

	if (GameGUICreatorHelpers::DrawTextureCombo("Texture", widget.texture, true, "<No Texture>"))
	{
		// Image widgets should resize to match the selected texture so the editor preview stays faithful.
		if (GameGUICreatorHelpers::RefreshTextureBaseline(widget, widget.texture, false))
		{
			const int previousWidth = widget.width;
			const int previousHeight = widget.height;
			widget.width = widget.defaultWidth;
			widget.height = widget.defaultHeight;
			widget.x += (previousWidth - widget.width) / 2;
			widget.y += (previousHeight - widget.height) / 2;
		}
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

	if (ImGui::Button("Delete"))
	{
		DeleteSelectedWidget();
		SyncRuntimePreview();
	}
}
