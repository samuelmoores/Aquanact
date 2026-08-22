#include "Engine/UI/EngineMenuBar.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/FileManager.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>
#include <algorithm>

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

	FileManager& fileManager = *context.fileManager;
	SceneManager& sceneManager = *context.sceneManager;
	ProjectManager& projectManager = *context.projectManager;

	DrawAquanactMenu(context.window, windowState.showInputMapWindow);
	DrawFileMenu(fileManager, sceneManager, projectManager);
	result = DrawViewMenu(showAxis, showGrid, windowState);
	DrawLightingMenu();
	DrawGameMenu(projectManager, sceneManager, popupRequests.buildGame);
	DrawSceneMenu(sceneManager, popupRequests.newScene);
	DrawCodeMenu(popupRequests.addCodeFile, popupRequests.deleteComponent);
	DrawUiMenu();

	ImGui::EndMainMenuBar();
	return result;
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
	EngineGuiWidgets::ToggleMenuItem("Camera Window", windowState.showCameraWindow);
	EngineGuiWidgets::ToggleMenuItem("Audio Window", windowState.showAudioWindow);

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
	bool showFileExplorer = windowState.showFileExplorer;
	if (EngineGuiWidgets::ToggleMenuItem("File Explorer", showFileExplorer))
	{
		result.showFileExplorer = showFileExplorer;
	}
	bool showSceneWindow = windowState.showSceneWindow;
	if (EngineGuiWidgets::ToggleMenuItem("Scene Window", showSceneWindow))
	{
		result.showSceneWindow = showSceneWindow;
	}
	bool showEntityWindow = windowState.showEntityWindow;
	if (EngineGuiWidgets::ToggleMenuItem("Entity Window", showEntityWindow))
	{
		result.showEntityWindow = showEntityWindow;
	}
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
	if (EngineGuiWidgets::ToggleMenuItem("Stats Window", showStatsWindow))
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

void EngineMenuBar::DrawAquanactMenu(Window* window, bool& showInputMap) const
{
	if (!ImGui::BeginMenu("Aquanact"))
	{
		return;
	}

	if (ImGui::MenuItem("Input Map"))
	{
		showInputMap = true;
	}
	if (ImGui::MenuItem("Quit") && window)
	{
		glfwSetWindowShouldClose(window->GLFW(), GLFW_TRUE);
	}
	ImGui::EndMenu();
}

void EngineMenuBar::DrawFileMenu(
	FileManager& fileManager,
	SceneManager& sceneManager,
	ProjectManager& projectManager) const
{
	if (!ImGui::BeginMenu("File"))
	{
		return;
	}

	if (ImGui::MenuItem("Save Project"))
	{
		projectManager.SaveProject(projectManager.CurrentProjectPath(), sceneManager);
	}
	if (ImGui::MenuItem("Load Project"))
	{
		projectManager.LoadProject(projectManager.CurrentProjectPath(), sceneManager);
	}

	ImGui::Separator();
	if (ImGui::MenuItem("Import Selected", nullptr, false, fileManager.CanImportSelection()))
	{
		fileManager.ImportSelected();
	}
	ImGui::EndMenu();
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
	SceneManager& sceneManager,
	bool& buildGameRequested) const
{
	if (!ImGui::BeginMenu("Game"))
	{
		return;
	}

	if (ImGui::BeginMenu("Camera"))
	{
		const bool thirdPerson = Root::Current().Render().CameraModeValue() == RenderManager::CameraMode::ThirdPerson;
		if (ImGui::MenuItem("Third Person", nullptr, thirdPerson))
		{
			Root::Current().Render().SetCameraMode(RenderManager::CameraMode::ThirdPerson);
		}
		ImGui::EndMenu();
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

	if (ImGui::MenuItem("Set Game Camera"))
	{
		Root::Current().Render().GetPathedCamera().SetPose(
			Root::Current().Render().GetEngineCamera().GetPosition(),
			Root::Current().Render().GetEngineCamera().GetFacing());
	}
	if (ImGui::MenuItem("Build Game"))
	{
		buildGameRequested = true;
	}
	ImGui::EndMenu();
}
