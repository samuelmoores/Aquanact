#include "Engine/UI/ComponentDeletionWindow.h"

#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/UI/EngineGuiWidgets.h"
#include "Engine/UI/GameCodeMaintenance.h"

#include <imgui.h>

#include <memory>
#include <vector>

std::size_t ComponentDeletionWindow::RemoveLiveComponentsFromScene(Scene* scene, const std::string& componentName)
{
	if (!scene) return 0;
	std::size_t removedCount = 0;
	for (const std::unique_ptr<Entity>& entity : scene->Objects())
	{
		if (!entity) continue;
		const std::vector<Component*> components = entity->Components();
		for (Component* component : components)
		{
			if (component && component->Name() == componentName && entity->RemoveComponent(component))
				++removedCount;
		}
	}
	return removedCount;
}

std::size_t ComponentDeletionWindow::CountLiveComponentsInScene(const Scene* scene, const std::string& componentName)
{
	if (!scene) return 0;
	std::size_t count = 0;
	for (const std::unique_ptr<Entity>& entity : scene->Objects())
	{
		if (!entity) continue;
		for (const Component* component : entity->Components())
			if (component && component->Name() == componentName) ++count;
	}
	return count;
}

std::size_t ComponentDeletionWindow::RemoveLiveComponentsFromAllScenes(const SceneManager& sceneManager, const std::string& componentName)
{
	std::size_t removedCount = 0;
	for (const std::unique_ptr<Scene>& scene : sceneManager.Levels())
		if (scene) removedCount += RemoveLiveComponentsFromScene(scene.get(), componentName);
	return removedCount;
}

std::size_t ComponentDeletionWindow::CountLiveComponentsInAllScenes(const SceneManager& sceneManager, const std::string& componentName)
{
	std::size_t count = 0;
	for (const std::unique_ptr<Scene>& scene : sceneManager.Levels())
		if (scene) count += CountLiveComponentsInScene(scene.get(), componentName);
	return count;
}

void ComponentDeletionWindow::DeleteComponentType(SceneManager& sceneManager, const std::string& componentName)
{
	const std::size_t removedInstances = RemoveLiveComponentsFromAllScenes(sceneManager, componentName);
	const bool removedFromRegistry = ComponentFactory::Instance().Unregister(componentName);
	const bool removedFiles = GameCodeMaintenance::DeleteComponentFiles(componentName);
	const bool buildFilesUpdated = GameCodeMaintenance::RegenerateBuildFiles();
	if (removedInstances > 0 || removedFromRegistry || removedFiles || !buildFilesUpdated)
	{
		Root::Current().Debugger().LogMessage(
			"Component type deleted: " + componentName + " (removed " + std::to_string(removedInstances) + " live instances)");
	}
}

void ComponentDeletionWindow::Draw(SceneManager& sceneManager, bool& popupRequested)
{
	EngineGuiWidgets::ConsumePopupRequest("Delete Component Type##AquanactDeleteComponentType", popupRequested);
	EngineGuiWidgets::ModalScope popup("Delete Component Type##AquanactDeleteComponentType");
	if (!popup) return;

	const std::vector<std::string> componentNames = ComponentFactory::Instance().Names();
	int selectedIndex = -1;
	for (int index = 0; index < static_cast<int>(componentNames.size()); ++index)
	{
		if (componentNames[static_cast<std::size_t>(index)] == m_selectedComponent)
		{
			selectedIndex = index;
			break;
		}
	}
	if (EngineGuiWidgets::StringCombo("Component Type", selectedIndex, componentNames, "<select component>")
		&& selectedIndex >= 0)
	{
		m_selectedComponent = componentNames[static_cast<std::size_t>(selectedIndex)];
	}

	const bool canDelete = !m_selectedComponent.empty();
	const std::size_t liveInstanceCount = canDelete
		? CountLiveComponentsInAllScenes(sceneManager, m_selectedComponent) : 0;
	if (canDelete)
	{
		ImGui::Text("Delete %s?", m_selectedComponent.c_str());
		if (liveInstanceCount == 1)
			ImGui::Text("1 live instance will be removed from loaded scenes.");
		else
			ImGui::Text("%zu live instances will be removed from loaded scenes.", liveInstanceCount);
	}
	else
	{
		ImGui::TextDisabled("Select a component type to see how many live instances will be removed.");
	}

	const EngineGuiWidgets::DialogAction action =
		EngineGuiWidgets::ConfirmationButtons("Delete", "Cancel", canDelete);
	if (action == EngineGuiWidgets::DialogAction::Confirm)
	{
		DeleteComponentType(sceneManager, m_selectedComponent);
		m_selectedComponent.clear();
		popupRequested = false;
		ImGui::CloseCurrentPopup();
	}
	else if (action == EngineGuiWidgets::DialogAction::Cancel)
	{
		m_selectedComponent.clear();
		popupRequested = false;
		ImGui::CloseCurrentPopup();
	}
}
