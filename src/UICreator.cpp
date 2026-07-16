#include "UICreator.h"

#include "FrontEndManager.h"
#include "RenderManager.h"
#include "Debug.h"
#include "Window.h"
#include "Camera.h"
#include "Globals.h"

#include <imgui.h>

void UICreator::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	m_window = &window;
	m_initialized = true;
}

void UICreator::shutDown()
{
	m_window = nullptr;
	m_initialized = false;
	m_showCreatePopup = false;
}

void UICreator::BeginFrame()
{
}

void UICreator::Draw(const Camera&)
{
	if (!m_initialized)
	{
		return;
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("UI"))
		{
			if (ImGui::MenuItem("Create Button"))
			{
				gDebug.LogMessage("Create Button requested");
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Leave UI Creation"))
			{
				gFrontEndManager.ReturnToEngineEditor();
				gRenderManager.SetActiveCamera(gRenderManager.GetEngineCamera());
				gDebug.LogMessage("Leave UI Creation requested");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			EngineGUI& engineGUI = gFrontEndManager.EditorGUI();
			bool showAxis = engineGUI.ShowAxis();
			bool showGrid = engineGUI.ShowGrid();
			ImGui::MenuItem("Axis", nullptr, &showAxis);
			if (showAxis != engineGUI.ShowAxis())
			{
				engineGUI.SetShowAxis(showAxis);
			}
			ImGui::Separator();
			ImGui::MenuItem("Show Grid", nullptr, &showGrid);
			if (showGrid != engineGUI.ShowGrid())
			{
				engineGUI.SetShowGrid(showGrid);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::Begin("UI Creator");
	ImGui::TextUnformatted("UI creation editor");
	ImGui::TextUnformatted("Use the top menu to create or exit.");
	ImGui::End();
}

void UICreator::EndFrame()
{
}
