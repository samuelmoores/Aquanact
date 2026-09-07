#include "Engine/UI/EngineMenuBar.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>
#include <string>

namespace
{
	char g_newProjectName[128] = "NewProject";
	bool g_openNewProjectPopup = false;
	// The editor starts with the project selector open. Canceling leaves the
	// editor in its empty startup state.
	bool g_openLoadProjectPopup = true;
	std::filesystem::path g_selectedProjectPath;
}

EngineMenuBarResult EngineMenuBar::Draw(
	const EngineGuiFrameContext& context,
	bool& showAxis,
	bool& showGrid,
	EngineGuiWindowState& windowState,
	EngineGuiPopupRequests& popupRequests) const
{
	EngineMenuBarResult result;
	if (!context.fileManager || !context.sceneManager || !context.projectManager)
	{
		return result;
	}

	if (!ImGui::BeginMainMenuBar())
	{
		return result;
	}

	SceneManager& sceneManager = *context.sceneManager;
	ProjectManager& projectManager = *context.projectManager;

	DrawAquanactMenu(context.window, windowState.showInputMapWindow, popupRequests.buildGame);
	DrawFileMenu(sceneManager, projectManager);
	result = DrawViewMenu(showAxis, showGrid, windowState);
	DrawLightingMenu();
	DrawGameMenu(projectManager, sceneManager);
	DrawSceneMenu(sceneManager, popupRequests.newScene);
	DrawCodeMenu(popupRequests.addCodeFile, popupRequests.deleteComponent);
	DrawUiMenu();

	ImGui::EndMainMenuBar();
	if (g_openNewProjectPopup)
	{
		ImGui::OpenPopup("New Project");
		g_openNewProjectPopup = false;
	}
	if (g_openLoadProjectPopup)
	{
		ImGui::OpenPopup("Load Project");
		g_openLoadProjectPopup = false;
	}
	DrawNewProjectDialog(sceneManager, projectManager);
	DrawLoadProjectDialog(sceneManager, projectManager);
	return result;
}

void EngineMenuBar::DrawLoadProjectDialog(
	SceneManager& sceneManager,
	ProjectManager& projectManager) const
{
	if (!ImGui::BeginPopupModal("Load Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	const std::filesystem::path projectFolder = projectManager.ProjectsRoot();
	std::vector<std::filesystem::path> projectFiles;
	std::error_code error;
	if (std::filesystem::exists(projectFolder, error))
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(projectFolder, error))
		{
			if (error)
			{
				break;
			}
			if (entry.is_regular_file(error) && entry.path().extension() == ".aqua")
			{
				projectFiles.push_back(entry.path());
			}
		}
	}
	std::sort(projectFiles.begin(), projectFiles.end());

	ImGui::Text("Project files in %s", projectFolder.string().c_str());
	ImGui::BeginChild("ProjectFileList", ImVec2(420.0f, 220.0f), true);
	if (projectFiles.empty())
	{
		ImGui::TextDisabled("No .aqua project files found.");
	}
	else
	{
		for (const auto& projectFile : projectFiles)
		{
			const bool selected = projectFile == g_selectedProjectPath;
			const std::filesystem::path displayPath = std::filesystem::relative(projectFile, projectFolder, error);
			const std::string label = error ? projectFile.filename().string() : displayPath.generic_string();
			if (ImGui::Selectable(label.c_str(), selected))
			{
				g_selectedProjectPath = projectFile;
			}
		}
	}
	ImGui::EndChild();

	const bool canLoad = !g_selectedProjectPath.empty();
	if (!canLoad)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Load") && projectManager.LoadProject(g_selectedProjectPath, sceneManager))
	{
		sceneManager.startUp();
		g_selectedProjectPath.clear();
		ImGui::CloseCurrentPopup();
	}
	if (!canLoad)
	{
		ImGui::EndDisabled();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		g_selectedProjectPath.clear();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void EngineMenuBar::DrawNewProjectDialog(
	SceneManager& sceneManager,
	ProjectManager& projectManager) const
{
	if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Create a self-contained project with its own assets folder.");
		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}
		const bool submitted = ImGui::InputText(
			"Project name",
			g_newProjectName,
			sizeof(g_newProjectName),
			ImGuiInputTextFlags_EnterReturnsTrue);

		std::string name(g_newProjectName);
		const bool validName = !name.empty() &&
			name.find_first_of("\\/:*?\"<>|") == std::string::npos;
		const std::filesystem::path projectPath = projectManager.ProjectsRoot() / name / "project.aqua";
		const bool alreadyExists = validName && std::filesystem::exists(projectPath);

		if (alreadyExists)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "A project with this name already exists.");
		}

		const bool canCreate = validName && !alreadyExists;
		if (!canCreate)
		{
			ImGui::BeginDisabled();
		}
		const bool createPressed = ImGui::Button("Create");
		if ((submitted || createPressed) && canCreate &&
			projectManager.CreateNewProject(projectPath, sceneManager))
		{
			ImGui::CloseCurrentPopup();
		}
		if (!canCreate)
		{
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

EngineMenuBarResult EngineMenuBar::DrawViewMenu(
	bool& showAxis,
	bool& showGrid,
	EngineGuiWindowState& windowState) const
{
	EngineMenuBarResult result;
	if (!ImGui::BeginMenu("View"))
	{
		return result;
	}

	ImGui::TextDisabled("Engine");
	ImGui::Separator();
	EngineGuiWidgets::ToggleMenuItem("Axis", showAxis);

	if (ImGui::BeginMenu("EngineCamera"))
	{
		float moveSpeed = Root::Current().Render().GetEngineCamera().MoveSpeed();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f"))
		{
			Root::Current().Render().GetEngineCamera().SetMoveSpeed(moveSpeed);
		}
		float lookSensitivity = Root::Current().Render().GetEngineCamera().LookSensitivity();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputFloat("Look Sensitivity", &lookSensitivity, 0.0f, 0.0f, "%.3f"))
		{
			Root::Current().Render().GetEngineCamera().SetLookSensitivity(lookSensitivity);
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Grid"))
	{
		EngineGuiWidgets::ToggleMenuItem("Show Grid", showGrid);
		float gridSize = Root::Current().Debugger().GridSize();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::DragFloat("Size", &gridSize, 1.0f, 1.0f, 10000.0f, "%.1f"))
		{
			Root::Current().Debugger().SetGridSettings(std::max(gridSize, 1.0f));
		}
		ImGui::EndMenu();
	}

	ImGui::Separator();
	ImGui::TextDisabled("Windows");
	ImGui::Separator();
	EngineGuiWidgets::ToggleMenuItem("Audio Window", windowState.showAudioWindow);
	EngineGuiWidgets::ToggleMenuItem("Camera Window", windowState.showCameraWindow);
	bool showEntityWindow = windowState.showEntityWindow;
	if (EngineGuiWidgets::ToggleMenuItem("Entity Window", showEntityWindow))
	{
		result.showEntityWindow = showEntityWindow;
	}
	bool showFileExplorer = windowState.showFileExplorer;
	if (EngineGuiWidgets::ToggleMenuItem("File Explorer", showFileExplorer))
	{
		result.showFileExplorer = showFileExplorer;
	}
	EngineGuiWidgets::ToggleMenuItem("Level Colliders Window", windowState.showLevelColliderWindow);
	bool showLightingWindow = windowState.showLightingWindow;
	if (EngineGuiWidgets::ToggleMenuItem("Lighting Window", showLightingWindow))
	{
		result.showLightingWindow = showLightingWindow;
	}

	bool showLogWindow = Root::Current().Debugger().ShowLogWindow();
	bool showStatsWindow = Root::Current().Debugger().ShowStatsWindow();
	if (EngineGuiWidgets::ToggleMenuItem("Log Window", showLogWindow))
	{
		Root::Current().Debugger().SetShowLogWindow(showLogWindow);
	}
	bool showSceneWindow = windowState.showSceneWindow;
	if (EngineGuiWidgets::ToggleMenuItem("Scene Window", showSceneWindow))
	{
		result.showSceneWindow = showSceneWindow;
	}
	if (EngineGuiWidgets::ToggleMenuItem("Render Stats Window", showStatsWindow))
	{
		Root::Current().Debugger().SetShowStatsWindow(showStatsWindow);
	}

	ImGui::EndMenu();
	return result;
}

void EngineMenuBar::DrawLightingMenu() const
{
	if (!ImGui::BeginMenu("Lighting"))
	{
		return;
	}

	const bool canAddPointLight =
		Root::Current().Render().Lights().PointLights().size() < LightingManager::MaxPointLights;
	if (ImGui::MenuItem("Add Point Light", nullptr, false, canAddPointLight))
	{
		Root::Current().Render().Lights().AddPointLight();
	}
	ImGui::EndMenu();
}

void EngineMenuBar::DrawAquanactMenu(Window* window, bool& showInputMap, bool& buildGameRequested) const
{
	if (!ImGui::BeginMenu("Aquanact"))
	{
		return;
	}

	if (ImGui::MenuItem("Input Map"))
	{
		showInputMap = true;
	}
	if (ImGui::MenuItem("Build Game"))
	{
		buildGameRequested = true;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Quit") && window)
	{
		glfwSetWindowShouldClose(window->GLFW(), GLFW_TRUE);
	}
	ImGui::EndMenu();
}

void EngineMenuBar::DrawFileMenu(
	SceneManager& sceneManager,
	ProjectManager& projectManager) const
{
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New Project"))
		{
			std::strncpy(g_newProjectName, "NewProject", sizeof(g_newProjectName));
			g_newProjectName[sizeof(g_newProjectName) - 1] = '\0';
			g_openNewProjectPopup = true;
		}

		if (ImGui::MenuItem("Save Project"))
		{
			projectManager.SaveProject(projectManager.CurrentProjectPath(), sceneManager);
		}
		if (ImGui::MenuItem("Load Project"))
		{
			g_selectedProjectPath.clear();
			g_openLoadProjectPopup = true;
		}
		ImGui::EndMenu();
	}
}

void EngineMenuBar::DrawSceneMenu(SceneManager& sceneManager, bool& newLevelRequested) const
{
	if (!ImGui::BeginMenu("Scene"))
	{
		return;
	}

	if (ImGui::BeginMenu("Levels"))
	{
		const std::vector<std::string> names = sceneManager.SceneNames(SceneManager::SceneKind::Level);
		for (const std::string& name : names)
		{
			const Scene* scene = sceneManager.FindLevel(name);
			if (ImGui::MenuItem(name.c_str(), nullptr, sceneManager.ActiveLevel() == scene))
			{
				sceneManager.SetActiveLevel(name);
				sceneManager.SetStartupLevelName(name);
			}
		}
		if (names.empty())
		{
			ImGui::MenuItem("No gameplay scenes created.", nullptr, false, false);
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Cutscenes"))
	{
		const std::vector<std::string> names = sceneManager.SceneNames(SceneManager::SceneKind::Cutscene);
		for (const std::string& name : names)
		{
			const Scene* scene = sceneManager.FindLevel(name);
			if (ImGui::MenuItem(name.c_str(), nullptr, sceneManager.ActiveLevel() == scene))
			{
				sceneManager.SetActiveLevel(name);
				sceneManager.SetStartupLevelName(name);
			}
		}
		if (names.empty())
		{
			ImGui::MenuItem("No cutscenes created.", nullptr, false, false);
		}
		ImGui::EndMenu();
	}

	if (ImGui::MenuItem("New Scene"))
	{
		newLevelRequested = true;
	}
	ImGui::EndMenu();
}

void EngineMenuBar::DrawCodeMenu(bool& addCodeFileRequested, bool& deleteComponentRequested) const
{
	if (!ImGui::BeginMenu("Code"))
	{
		return;
	}
	if (ImGui::MenuItem("Add Code File"))
	{
		addCodeFileRequested = true;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Delete Component Type"))
	{
		deleteComponentRequested = true;
	}
	ImGui::EndMenu();
}

void EngineMenuBar::DrawUiMenu() const
{
	if (!ImGui::BeginMenu("UI"))
	{
		return;
	}
	if (ImGui::MenuItem("GameGUI Creator"))
	{
		Root::Current().FrontEnd().OpenGameGUICreator();
		Root::Current().Render().SetGameMode();
		Root::Current().Debugger().LogMessage("GameGUI Creator opened.");
	}
	ImGui::EndMenu();
}

void EngineMenuBar::DrawGameMenu(
	ProjectManager& projectManager,
	SceneManager& sceneManager) const
{
	if (!ImGui::BeginMenu("Game"))
	{
		return;
	}

	if (ImGui::MenuItem("Play Game"))
	{
		Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
		Root::Current().FrontEnd().RuntimeGUI().ReloadAssetsFromDisk();
		if (projectManager.SaveProject(projectManager.CurrentProjectPath(), sceneManager))
		{
			Root::Current().FrontEnd().RestoreRuntimeLayout();
			Root::Current().State().SetMode(EngineMode::Game);
			Root::Current().EditorLaunchedGameSession() = true;
			Root::Current().Render().SetGameMode();
			Root::Current().Gameplay().startUp(sceneManager, Root::Current().FrontEnd(), Root::Current().Debugger(), Root::Current().State());
			Root::Current().Gameplay().BootMainMenu(Root::Current().FrontEnd(), Root::Current().Debugger());
		}
		else
		{
			Root::Current().Debugger().LogMessage("Play Game aborted because project autosave failed.");
		}
	}

	if (ImGui::MenuItem("Play Scene"))
	{
		Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
		Root::Current().FrontEnd().RuntimeGUI().ReloadAssetsFromDisk();
		if (projectManager.SaveProject(projectManager.CurrentProjectPath(), sceneManager))
		{
			Root::Current().FrontEnd().RestoreRuntimeLayout();
			Root::Current().State().SetMode(EngineMode::Game);
			Root::Current().EditorLaunchedGameSession() = true;
			Root::Current().Render().SetGameMode();
			Root::Current().Gameplay().startUp(sceneManager, Root::Current().FrontEnd(), Root::Current().Debugger(), Root::Current().State());
			Root::Current().Gameplay().StartGameSession(Root::Current().FrontEnd(), Root::Current().Debugger(), Root::Current().State());
		}
		else
		{
			Root::Current().Debugger().LogMessage("Play Scene aborted because project autosave failed.");
		}
	}

	ImGui::EndMenu();
}
