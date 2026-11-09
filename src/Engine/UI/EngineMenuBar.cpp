#include "Engine/UI/EngineMenuBar.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/FileSystem.h"
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
	// File -> Load Project uses this popup after normal editor startup. The
	// initial project-only screen has its own persistent window below.
	bool g_openLoadProjectPopup = false;
	std::filesystem::path g_selectedProjectPath;
	char g_projectPathInput[1024] = {};
	char g_projectDirectoryInput[1024] = {};
	std::filesystem::path g_projectExplorerDirectory;
	std::filesystem::path g_projectExplorerSelectedDirectory;
	std::filesystem::path g_startupProjectDeleteDirectory;
	std::filesystem::path g_startupSelectedProjectFile;
	bool g_showStartupNewProjectForm = false;
	bool g_confirmStartupProjectDelete = false;
	bool g_startupProjectPreferenceApplied = false;
	bool IsValidProjectPath(const std::filesystem::path& path);

	std::string UniqueEmptyEntityName(const Scene& scene)
	{
		const std::string baseName = "Empty Entity";
		if (!scene.FindObjectByName(baseName))
		{
			return baseName;
		}
		for (std::size_t suffix = 2;; ++suffix)
		{
			const std::string candidate = baseName + " " + std::to_string(suffix);
			if (!scene.FindObjectByName(candidate))
			{
				return candidate;
			}
		}
	}

	std::filesystem::path ProjectPreferencePath()
	{
		return Root::Current().FileSystemRef().ExecutableDirectory() / "aquanact_last_project.txt";
	}

	std::filesystem::path LoadSavedProjectPath()
	{
		std::string savedPath = Root::Current().FileSystemRef().ReadTextFile(ProjectPreferencePath());
		while (!savedPath.empty() && (savedPath.back() == '\r' || savedPath.back() == '\n'))
		{
			savedPath.pop_back();
		}
		const std::filesystem::path path(savedPath);
		return IsValidProjectPath(path) ? path : std::filesystem::path{};
	}

	void SaveProjectPath(const std::filesystem::path& path)
	{
		if (IsValidProjectPath(path))
		{
			Root::Current().FileSystemRef().WriteTextFile(ProjectPreferencePath(), path.string() + "\n");
		}
	}

	void SetProjectPathInput(const std::filesystem::path& path)
	{
		const std::string text = path.string();
		std::strncpy(g_projectPathInput, text.c_str(), sizeof(g_projectPathInput) - 1);
		g_projectPathInput[sizeof(g_projectPathInput) - 1] = '\0';
	}

	bool IsValidProjectPath(const std::filesystem::path& path)
	{
		std::error_code error;
		return !path.empty() &&
			path.extension() == ".aqua" &&
			std::filesystem::is_regular_file(path, error);
	}

	void SetProjectDirectoryInput(const std::filesystem::path& path)
	{
		const std::string text = path.string();
		std::strncpy(g_projectDirectoryInput, text.c_str(), sizeof(g_projectDirectoryInput) - 1);
		g_projectDirectoryInput[sizeof(g_projectDirectoryInput) - 1] = '\0';
	}

	bool IsValidProjectDirectory(const std::filesystem::path& path)
	{
		std::error_code error;
		return !path.empty() && std::filesystem::is_directory(path, error);
	}

	void SetProjectExplorerDirectory(const std::filesystem::path& path)
	{
		if (!IsValidProjectDirectory(path))
		{
			return;
		}
		g_projectExplorerDirectory = std::filesystem::absolute(path).lexically_normal();
		g_projectExplorerSelectedDirectory.clear();
		SetProjectDirectoryInput(g_projectExplorerDirectory);
	}
}

std::filesystem::path EngineMenuBar::DrawStartupProjectWindow(const EngineGuiFrameContext& context) const
{
	if (!context.projectManager || !context.sceneManager)
	{
		return {};
	}

	ProjectManager& projectManager = *context.projectManager;
	SceneManager& sceneManager = *context.sceneManager;
	std::filesystem::path projectToOpen;
	if (!g_startupProjectPreferenceApplied)
	{
		g_startupProjectPreferenceApplied = true;
		g_projectExplorerDirectory = std::filesystem::path("C:/projects");
		g_selectedProjectPath.clear();
		g_startupSelectedProjectFile.clear();
	}

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::GetBackgroundDrawList()->AddRectFilled(
		viewport->Pos,
		ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
		IM_COL32(5, 5, 7, 255));
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_Always);
	ImGui::Begin("Projects", nullptr,
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	std::vector<std::pair<std::filesystem::path, std::filesystem::path>> projectFolders;
	std::error_code error;
	if (IsValidProjectDirectory(g_projectExplorerDirectory))
	{
		const auto end = std::filesystem::directory_iterator();
		for (std::filesystem::directory_iterator it(
			g_projectExplorerDirectory,
			std::filesystem::directory_options::skip_permission_denied,
			error);
			!error && it != end;
			it.increment(error))
		{
			const std::filesystem::directory_entry& entry = *it;
			if (!entry.is_directory(error))
			{
				error.clear();
				continue;
			}
			const std::filesystem::path folder = entry.path();
			error.clear();
			std::filesystem::path projectFile;
			for (std::filesystem::directory_iterator child(folder,
				std::filesystem::directory_options::skip_permission_denied, error);
				!error && child != end;
				child.increment(error))
			{
				if (child->is_regular_file(error) && child->path().extension() == ".aqua")
				{
					projectFile = child->path();
					break;
				}
				error.clear();
			}
			error.clear();
			if (!projectFile.empty())
			{
				projectFolders.emplace_back(folder, projectFile);
			}
		}
	}
	std::sort(projectFolders.begin(), projectFolders.end(), [](const auto& left, const auto& right)
	{
		return left.first.filename().string() < right.first.filename().string();
	});

	ImGui::Text("Projects: %s", g_projectExplorerDirectory.string().c_str());
	ImGui::BeginChild("StartupProjectFileList", ImVec2(0.0f, 300.0f), true);
	if (projectFolders.empty())
	{
		ImGui::TextDisabled("No project folders containing a .aqua file were found.");
	}
	else
	{
		for (const auto& project : projectFolders)
		{
			const auto& folder = project.first;
			ImGui::PushID(folder.generic_string().c_str());
			if (ImGui::Selectable(folder.filename().string().c_str(), g_startupSelectedProjectFile == project.second))
			{
				g_startupProjectDeleteDirectory = folder;
				g_startupSelectedProjectFile = project.second;
				g_selectedProjectPath = project.second;
				SetProjectPathInput(project.second);
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
	const bool hasSelection = !g_startupProjectDeleteDirectory.empty() &&
		!g_startupSelectedProjectFile.empty();
	if (hasSelection)
	{
		if (ImGui::Button("Open Project") && IsValidProjectPath(g_startupSelectedProjectFile))
		{
			projectToOpen = g_startupSelectedProjectFile;
			SaveProjectPath(projectToOpen);
		}
		ImGui::SameLine();
		if (ImGui::Button(g_confirmStartupProjectDelete ? "Confirm Delete" : "Delete Project"))
		{
			if (!g_confirmStartupProjectDelete)
			{
				g_confirmStartupProjectDelete = true;
			}
			else
			{
				std::error_code deleteError;
				std::filesystem::remove_all(g_startupProjectDeleteDirectory, deleteError);
				if (!deleteError)
				{
					g_startupProjectDeleteDirectory.clear();
					g_startupSelectedProjectFile.clear();
					g_selectedProjectPath.clear();
					g_projectPathInput[0] = '\0';
					g_confirmStartupProjectDelete = false;
				}
			}
		}
		if (g_confirmStartupProjectDelete)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Cancel"))
			{
				g_confirmStartupProjectDelete = false;
			}
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Delete this project folder and all its files?");
		}
	}
	if (!g_showStartupNewProjectForm)
	{
		if (ImGui::Button("New Project"))
		{
			std::strncpy(g_newProjectName, "NewProject", sizeof(g_newProjectName));
			g_newProjectName[sizeof(g_newProjectName) - 1] = '\0';
			g_showStartupNewProjectForm = true;
		}
	}
	else
	{
		ImGui::Separator();
		ImGui::TextUnformatted("New project name");
		const bool submitted = ImGui::InputText("##StartupNewProjectName", g_newProjectName,
			sizeof(g_newProjectName), ImGuiInputTextFlags_EnterReturnsTrue);
		const std::string name(g_newProjectName);
		const bool validName = !name.empty() && name.find_first_of("\\/:*?\"<>|") == std::string::npos;
		const std::filesystem::path projectPath = g_projectExplorerDirectory / name / "project.aqua";
		const bool canCreate = validName && !std::filesystem::exists(projectPath);
		bool createPressed = false;
		if (!canCreate) ImGui::BeginDisabled();
		createPressed = ImGui::Button("Create");
		if ((submitted || createPressed) && canCreate &&
			projectManager.CreateNewProject(projectPath, sceneManager))
		{
			g_showStartupNewProjectForm = false;
			g_startupProjectDeleteDirectory = projectPath.parent_path();
			g_startupSelectedProjectFile = projectPath;
			g_selectedProjectPath = projectPath;
			SetProjectPathInput(projectPath);
		}
		if (!canCreate) ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) g_showStartupNewProjectForm = false;
	}
	ImGui::End();
	return projectToOpen;
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

	bool openProjectExplorer = false;
	DrawAquanactMenu(context.window, windowState.showInputMapWindow, popupRequests.buildGame, openProjectExplorer);
	DrawFileMenu(sceneManager, projectManager);
	result = DrawViewMenu(showAxis, showGrid, windowState);
	result.openProjectExplorer = openProjectExplorer;
	DrawGameMenu(projectManager, sceneManager);
	DrawSceneMenu(context, windowState, popupRequests.newScene);
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
	(void)DrawNewProjectDialog(sceneManager, projectManager);
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
				SetProjectPathInput(projectFile);
			}
		}
	}
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::TextUnformatted("Project file");
	if (ImGui::InputText("##LoadProjectPath", g_projectPathInput, sizeof(g_projectPathInput)))
	{
		g_selectedProjectPath = std::filesystem::path(g_projectPathInput);
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		g_selectedProjectPath = std::filesystem::path(g_projectPathInput);
	}
	const bool validTypedProject = IsValidProjectPath(g_selectedProjectPath);
	if (!g_selectedProjectPath.empty() && !validTypedProject)
	{
		ImGui::TextDisabled("Enter the full path to an existing .aqua file.");
	}

	const bool canLoad = validTypedProject;
	if (!canLoad)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Load") && projectManager.LoadProject(g_selectedProjectPath, sceneManager))
	{
		sceneManager.startUp();
		SaveProjectPath(g_selectedProjectPath);
		g_selectedProjectPath.clear();
		g_projectPathInput[0] = '\0';
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
		g_projectPathInput[0] = '\0';
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

bool EngineMenuBar::DrawNewProjectDialog(
	SceneManager& sceneManager,
	ProjectManager& projectManager) const
{
	bool projectCreated = false;
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
		const std::filesystem::path projectDirectory = IsValidProjectDirectory(g_projectExplorerDirectory)
			? g_projectExplorerDirectory
			: projectManager.ProjectsRoot();
		const std::filesystem::path projectPath = projectDirectory / name / "project.aqua";
		ImGui::Text("Location: %s", projectDirectory.string().c_str());
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
			projectCreated = true;
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
	return projectCreated;
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
	EngineGuiWidgets::ToggleMenuItem("Instance Manager", windowState.showInstanceManagerWindow);
	EngineGuiWidgets::ToggleMenuItem("SpawnManager", windowState.showSpawnManagerWindow);
	EngineGuiWidgets::ToggleMenuItem("Particle System", windowState.showParticleSystemWindow);
	EngineGuiWidgets::ToggleMenuItem("Shader Window", windowState.showShaderWindow);
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
	EngineGuiWidgets::ToggleMenuItem("Cutscene Timeline", windowState.showCutsceneWindow);

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

void EngineMenuBar::DrawAquanactMenu(Window* window, bool& showInputMap, bool& buildGameRequested, bool& openProjectExplorer) const
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
	if (ImGui::MenuItem("Project Explorer"))
	{
		openProjectExplorer = true;
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

void EngineMenuBar::DrawSceneMenu(
	const EngineGuiFrameContext& context,
	EngineGuiWindowState& windowState,
	bool& newLevelRequested) const
{
	if (!ImGui::BeginMenu("Scene"))
	{
		return;
	}
	SceneManager& sceneManager = *context.sceneManager;
	Scene* activeScene = sceneManager.ActiveLevel();
	const bool canAddPointLight = activeScene &&
		activeScene->Lights().PointLights().size() < LightingManager::MaxPointLights;
	if (ImGui::MenuItem("Add Point Light", nullptr, false, canAddPointLight))
	{
		activeScene->Lights().AddPointLight();
	}
	if (ImGui::MenuItem("Add Entity", nullptr, false, activeScene != nullptr))
	{
		auto entity = std::make_unique<Entity>(UniqueEmptyEntityName(*activeScene), false);
		Entity* addedEntity = activeScene->AddObject(std::move(entity));
		if (addedEntity && context.selection)
		{
			context.selection->entityId = addedEntity->Id();
			context.selection->pointLightIndex = -1;
			context.selection->levelColliderIndex = -1;
			windowState.showEntityWindow = true;
		}
	}
	ImGui::Separator();

	if (ImGui::BeginMenu("Levels"))
	{
		const std::vector<std::string> names = sceneManager.SceneNames(SceneManager::SceneKind::Level);
		for (const std::string& name : names)
		{
			const Scene* scene = sceneManager.FindLevel(name);
			if (ImGui::MenuItem(name.c_str(), nullptr, sceneManager.ActiveLevel() == scene))
			{
				// Flush GUI edits before changing the active scene. This is especially
				// important for New Game destinations, which are edited in the GUI
				// creator but should not be lost during a scene transition.
				Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
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
				// Flush GUI edits before changing the active scene. This is especially
				// important for New Game destinations, which are edited in the GUI
				// creator but should not be lost during a scene transition.
				Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
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

	bool fullscreen = Root::Current().WindowRef().IsFullscreen();
	if (ImGui::MenuItem("Fullscreen", nullptr, fullscreen))
		Root::Current().WindowRef().ToggleFullscreen();
	ImGui::Separator();

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
