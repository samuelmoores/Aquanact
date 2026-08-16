#include "Engine/UI/CodeCreationWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Window.h"
#include "Engine/UI/EngineGuiWidgets.h"
#include "Engine/UI/GameCodeMaintenance.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <cctype>
#include <fstream>
#include <vector>

std::string CodeCreationWindow::NormalizeClassName(const std::string& input)
{
	std::string output;
	bool capitalizeNext = true;
	for (unsigned char ch : input)
	{
		if (std::isalnum(ch))
		{
			output.push_back(capitalizeNext ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch));
			capitalizeNext = false;
		}
		else capitalizeNext = true;
	}
	return output;
}

void CodeCreationWindow::SaveNewClassConfiguration(const std::string& className, bool attachToExistingEntity, const std::string& targetEntityName)
{
	std::ofstream outFile("NewClassConfiguration");
	if (!outFile) return;
	outFile << "ClassName: " << className << "\n"
		<< "AttachToExistingEntity: " << std::boolalpha << attachToExistingEntity << "\n"
		<< "CreateNewEntity: " << std::boolalpha << !attachToExistingEntity << "\n"
		<< "TargetEntityName: " << targetEntityName << "\n";
}

void CodeCreationWindow::Draw(Window* window, bool& popupRequested, ProjectManager& projectManager)
{
	if (popupRequested)
	{
		m_newCodeFileName[0] = '\0';
		m_statusMessage.clear();
		m_createAndBuildClassName.clear();
		m_selectedEntityIndex = 0;
	}
	EngineGuiWidgets::ConsumePopupRequest("Add Code File##AquanactAddCodeFile", popupRequested);

	if (!ImGui::BeginPopupModal("Add Code File##AquanactAddCodeFile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
	ImGui::TextUnformatted("Create a new gameplay class:");
	ImGui::InputText("Class Name", m_newCodeFileName, sizeof(m_newCodeFileName));

	Scene* activeScene = Root::Current().Scenes().ActiveLevel();
	static const std::vector<std::unique_ptr<Entity>> emptyEntities;
	const auto& entities = activeScene ? activeScene->Objects() : emptyEntities;
	std::vector<std::string> entityNames{ "none" };
	for (const auto& entity : entities) entityNames.push_back(entity ? entity->Name() : "<unnamed>");
	if (m_selectedEntityIndex < 0 || m_selectedEntityIndex >= static_cast<int>(entityNames.size())) m_selectedEntityIndex = 0;
	EngineGuiWidgets::StringCombo("Entity", m_selectedEntityIndex, entityNames, "none");

	Entity* selectedEntity = (m_selectedEntityIndex > 0 && static_cast<std::size_t>(m_selectedEntityIndex - 1) < entities.size())
		? entities[static_cast<std::size_t>(m_selectedEntityIndex - 1)].get() : nullptr;
	if (ImGui::Button("Create and Build"))
	{
		m_createAndBuildClassName = NormalizeClassName(m_newCodeFileName);
		if (m_createAndBuildClassName.empty()) m_statusMessage = "Enter a valid class name.";
		else { ImGui::OpenPopup("Create and Build##AquanactCreateAndBuild"); m_createAndBuildPopupRequested = false; }
	}
	if (ImGui::BeginPopupModal("Create and Build##AquanactCreateAndBuild", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const bool attach = selectedEntity != nullptr;
		ImGui::Text("Create a new component type: %s", m_createAndBuildClassName.c_str());
		ImGui::Text("Selected entity: %s", selectedEntity ? selectedEntity->Name().c_str() : "none");
		ImGui::TextWrapped("The editor will generate the new class, save the project, and then shut down so you can rebuild the game.");
		ImGui::TextWrapped("Please rebuild after the editor closes to compile the new type into the project.");
		const EngineGuiWidgets::DialogAction action = EngineGuiWidgets::ConfirmationButtons("Create and Exit");
		if (action == EngineGuiWidgets::DialogAction::Confirm)
		{
			m_statusMessage = GameCodeMaintenance::CreateComponent(m_createAndBuildClassName).statusMessage;
			SaveNewClassConfiguration(m_createAndBuildClassName, attach, selectedEntity ? selectedEntity->Name() : "");
			if (!projectManager.CurrentProjectPath().empty()) projectManager.SaveProject(projectManager.CurrentProjectPath(), Root::Current().Scenes());
			if (window) glfwSetWindowShouldClose(window->GLFW(), GLFW_TRUE);
			ImGui::CloseCurrentPopup();
		}
		else if (action == EngineGuiWidgets::DialogAction::Cancel)
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
	EngineGuiWidgets::StatusMessage(m_statusMessage);
	ImGui::EndPopup();
}
