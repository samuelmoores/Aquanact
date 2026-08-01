#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Camera.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <functional>

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
				m_newWidgetIsImage = false;
				m_newWidgetIsProgressBar = false;
				m_newWidgetAction = GameGUIActionType::None;
				m_newWidgetLaunchLevel.clear();
				m_newWidgetName[0] = '\0';
				m_newWidgetTexture[0] = '\0';
			}
			if (ImGui::MenuItem("Create Image"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsImage = true;
				m_newWidgetIsProgressBar = false;
				m_newWidgetName[0] = '\0';
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "textures/example.png");
			}
			if (ImGui::MenuItem("Create Progress Bar"))
			{
				m_showCreateWidgetPopup = true;
				m_newWidgetIsImage = false;
				m_newWidgetIsProgressBar = true;
				m_newWidgetName[0] = '\0';
				std::snprintf(m_newWidgetTexture, sizeof(m_newWidgetTexture), "textures/example.png");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	DrawCreateWidgetPopup();
	DrawBindingPopup();
	DrawTexturePickerPopup();

	DrawWidgetList();
	DrawWidgetDetails();
}

