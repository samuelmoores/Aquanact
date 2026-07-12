#include "EngineGUI.h"

#include "FileManager.h"
#include "SceneManager.h"
#include "ProjectManager.h"
#include "Window.h"
#include "Camera.h"
#include "Object3D.h"

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

void EngineGUI::Draw(const Camera&, FileManager& fileManager, SceneManager& sceneManager, ProjectManager& projectManager)
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
			if (ImGui::MenuItem("Save Project"))
			{
				projectManager.SaveProject("C:/dev/Aquanact/assets/project.aqua", sceneManager);
			}
			if (ImGui::MenuItem("Load Project"))
			{
				projectManager.LoadProject("C:/dev/Aquanact/assets/project.aqua", sceneManager);
			}
			ImGui::Separator();
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
		if (entry.is_directory())
		{
			continue;
		}

		const std::filesystem::path entryPath = entry.path();
		const std::string label = entryPath.filename().string();
		const bool selected = fileManager.HasSelection() && fileManager.SelectedPath() == entryPath;

		if (ImGui::Selectable(label.c_str(), selected))
		{
			fileManager.SelectPath(entryPath);
		}
	}
	ImGui::End();

	ImGui::Begin("Scene");
	const auto& objects = sceneManager.Objects();
	for (size_t i = 0; i < objects.size(); ++i)
	{
		const auto& object = objects[i];
		const std::string label = object ? object->Name() : std::string("<null>");
		ImGui::BulletText("%zu: %s", i, label.empty() ? "<unnamed>" : label.c_str());
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
