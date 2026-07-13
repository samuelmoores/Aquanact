#include "EngineGUI.h"

#include "FileManager.h"
#include "Debug.h"
#include "Globals.h"
#include "SceneManager.h"
#include "ProjectManager.h"
#include "EngineCamera.h"
#include "Window.h"
#include "Camera.h"
#include "Object3D.h"
#include "AnimatorComponent.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>
#include <algorithm>

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
	const auto& objects = sceneManager.Objects();
	if (m_selectedSceneObjectIndex >= static_cast<int>(objects.size()))
	{
		m_selectedSceneObjectIndex = -1;
	}

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
			if (ImGui::BeginMenu("EngineCamera"))
			{
				float moveSpeed = gEngineCamera.MoveSpeed();
				ImGui::SetNextItemWidth(140.0f);
				if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f"))
				{
					gEngineCamera.SetMoveSpeed(moveSpeed);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Grid"))
			{
				ImGui::MenuItem("Show Grid", nullptr, &m_showGrid);
				float gridSize = gDebug.GridSize();
				float gridSpacing = gDebug.GridSpacing();
				ImGui::SetNextItemWidth(140.0f);
				bool sizeChanged = ImGui::InputFloat("Size", &gridSize, 0.0f, 0.0f, "%.1f");
				ImGui::SetNextItemWidth(140.0f);
				bool spacingChanged = ImGui::InputFloat("Spacing", &gridSpacing, 0.0f, 0.0f, "%.1f");
				gridSize = std::max(gridSize, 1.0f);
				gridSpacing = std::max(gridSpacing, 1.0f);
				if (sizeChanged || spacingChanged)
				{
					gDebug.SetGridSettings(gridSize, gridSpacing);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Project"))
			{
				projectManager.SaveProject("C:/dev/Aquanact/assets/projects/project.aqua", sceneManager);
			}
			if (ImGui::MenuItem("Load Project"))
			{
				projectManager.LoadProject("C:/dev/Aquanact/assets/projects/project.aqua", sceneManager);
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
	if (ImGui::Button("Models"))
	{
		fileManager.SetRootDirectory("C:/dev/Aquanact/assets/models");
	}
	ImGui::SameLine();
	if (ImGui::Button("Textures"))
	{
		fileManager.SetRootDirectory("C:/dev/Aquanact/assets/textures");
	}
	ImGui::SameLine();
	if (ImGui::Button("Projects"))
	{
		fileManager.SetRootDirectory("C:/dev/Aquanact/assets/projects");
	}

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
	for (size_t i = 0; i < objects.size(); ++i)
	{
		const auto& object = objects[i];
		const std::string label = object ? object->Name() : std::string("<null>");
		const bool selected = m_selectedSceneObjectIndex == static_cast<int>(i);
		if (ImGui::Selectable(label.empty() ? "<unnamed>" : label.c_str(), selected))
		{
			m_selectedSceneObjectIndex = static_cast<int>(i);
		}
	}
	ImGui::End();

	ImGui::Begin("Object");
	if (m_selectedSceneObjectIndex < 0 || m_selectedSceneObjectIndex >= static_cast<int>(objects.size()))
	{
		ImGui::TextUnformatted("No scene object selected.");
	}
	else
	{
		const auto& object = objects[static_cast<std::size_t>(m_selectedSceneObjectIndex)];
		if (!object)
		{
			ImGui::TextUnformatted("Selected object is null.");
		}
		else
		{
			const glm::vec3 worldCenterPosition = object->WorldCenterPosition();
			const glm::vec3 defaultWorldCenterPosition = object->InitialWorldCenterPosition();
			ImGui::Text("Name: %s", object->Name().empty() ? "<unnamed>" : object->Name().c_str());
			ImGui::Text("Components");
			const bool hasAnimator = object->HasAnimatorComponent();
			ImGui::BulletText("Animator: %s", hasAnimator ? "present" : "missing");

			float editedX = worldCenterPosition.x;
			float editedY = worldCenterPosition.y;
			float editedZ = worldCenterPosition.z;

			ImGui::TextUnformatted("World Center");
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragFloat("X##WorldCenter", &editedX, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				object->Translate(glm::vec3(editedX - worldCenterPosition.x, 0.0f, 0.0f));
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset X"))
			{
				object->Translate(glm::vec3(defaultWorldCenterPosition.x - worldCenterPosition.x, 0.0f, 0.0f));
				editedX = defaultWorldCenterPosition.x;
			}

			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragFloat("Y##WorldCenter", &editedY, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				object->Translate(glm::vec3(0.0f, editedY - worldCenterPosition.y, 0.0f));
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Y"))
			{
				object->Translate(glm::vec3(0.0f, defaultWorldCenterPosition.y - worldCenterPosition.y, 0.0f));
				editedY = defaultWorldCenterPosition.y;
			}

			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragFloat("Z##WorldCenter", &editedZ, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				object->Translate(glm::vec3(0.0f, 0.0f, editedZ - worldCenterPosition.z));
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Z"))
			{
				object->Translate(glm::vec3(0.0f, 0.0f, defaultWorldCenterPosition.z - worldCenterPosition.z));
				editedZ = defaultWorldCenterPosition.z;
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
