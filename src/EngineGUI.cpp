#include "EngineGUI.h"

#include "Window.h"
#include "Camera.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

void EngineGUI::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window.GLFW(), true);
	ImGui_ImplOpenGL3_Init("#version 330");
	m_initialized = true;
}

void EngineGUI::shutDown()
{
	if (!m_initialized)
	{
		return;
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	m_initialized = false;
}

void EngineGUI::BeginFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void EngineGUI::Draw(const Camera&)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Axis", nullptr, &m_showAxis);
			ImGui::MenuItem("Grid", nullptr, &m_showGrid);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void EngineGUI::EndFrame()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool EngineGUI::ShowAxis() const
{
	return m_showAxis;
}

bool EngineGUI::ShowGrid() const
{
	return m_showGrid;
}
