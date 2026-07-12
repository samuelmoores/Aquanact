#include "EngineGUI.h"

#include "FileManager.h"
#include "Window.h"
#include "Camera.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>

void EngineGUI::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	m_window = &window;
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
	m_window = nullptr;
}

void EngineGUI::BeginFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void EngineGUI::Draw(const Camera&, FileManager& fileManager)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Aquanact"))
		{
			if (ImGui::MenuItem("Quit"))
			{
				if (m_window)
				{
					glfwSetWindowShouldClose(m_window->GLFW(), GLFW_TRUE);
				}
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Axis", nullptr, &m_showAxis);
			ImGui::MenuItem("Grid", nullptr, &m_showGrid);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("File"))
		{
			const bool canImport = fileManager.CanImportSelection();
			if (ImGui::MenuItem("Import Selected", nullptr, false, canImport))
			{
				fileManager.ImportSelected();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::Begin("File Explorer");
	ImGui::Text("Root: %s", fileManager.RootDirectory().string().c_str());
	ImGui::Text("Current: %s", fileManager.CurrentDirectory().string().c_str());

	if (ImGui::Button("Up One Directory"))
	{
		fileManager.GoUpOneDirectory();
	}

	ImGui::Separator();
	ImGui::Text("Selection: %s", fileManager.SelectedPath().empty() ? "<none>" : fileManager.SelectedPath().string().c_str());

	if (fileManager.CanImportSelection())
	{
		if (ImGui::Button("Import Selected"))
		{
			fileManager.ImportSelected();
		}
	}

	ImGui::Separator();
	for (const auto& entry : fileManager.Entries())
	{
		const std::filesystem::path entryPath = entry.path();
		const std::string label = entry.is_directory() ? ("[DIR] " + entryPath.filename().string()) : entryPath.filename().string();
		const bool selected = fileManager.HasSelection() && fileManager.SelectedPath() == entryPath;

		if (ImGui::Selectable(label.c_str(), selected))
		{
			fileManager.SelectPath(entryPath);
			if (entry.is_directory())
			{
				fileManager.SetCurrentDirectory(entryPath);
			}
		}
	}
	ImGui::End();
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
