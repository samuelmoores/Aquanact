#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>
#include <vector>

namespace {
	Entity* FindEntity(Scene* scene, const std::string& name)
	{
		if (!scene)
		{
			return nullptr;
		}
		for (const auto& entity : scene->Entities())
		{
			if (entity && entity->Name() == name)
			{
				return entity.get();
			}
		}
		return nullptr;
	}
}

void GameGUICreator::DrawBindingPopup()
{
	if (m_showBindingPopup)
	{
		ImGui::OpenPopup("Add Binding");
		m_showBindingPopup = false;
	}

	if (!ImGui::BeginPopupModal("Add Binding", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	GameGUIAsset& asset = CurrentGameGUI();
	GameGUIWidgetDef* widget = nullptr;
	for (GameGUIWidgetDef& candidate : asset.widgets)
	{
		if (candidate.name == m_bindingWidgetName)
		{
			widget = &candidate;
			break;
		}
	}

	if (!widget)
	{
		ImGui::TextDisabled("Binding target not found.");
		if (ImGui::Button("Close"))
		{
			m_bindingWidgetName.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return;
	}

	ImGui::TextUnformatted("Binding target");
	ImGui::Text("Name: %s", widget->name.empty() ? "<Unnamed>" : widget->name.c_str());
	ImGui::Text("Type: %s", widget->type.empty() ? "<Unknown>" : widget->type.c_str());
	ImGui::Separator();

	Scene* activeLevel = Root::Current().Scenes().ActiveLevel();
	if (!activeLevel)
	{
		ImGui::TextDisabled("No active Scene is available.");
		if (ImGui::Button("Close"))
		{
			m_bindingWidgetName.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return;
	}

	const char* entityLabel = widget->bindEntity.empty() ? "<Select Entity>" : widget->bindEntity.c_str();
	if (ImGui::BeginCombo("Entity", entityLabel))
	{
		for (const auto& entity : activeLevel->Entities())
		{
			if (!entity)
			{
				continue;
			}
			const bool selected = widget->bindEntity == entity->Name();
			if (ImGui::Selectable(entity->Name().c_str(), selected))
			{
				widget->bindEntity = entity->Name();
				widget->bindComponent.clear();
				widget->bindMember.clear();
				widget->bindEvent.clear();
				SyncRuntimePreview();
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	Entity* boundEntity = FindEntity(activeLevel, widget->bindEntity);
	ImGui::BeginDisabled(!boundEntity);
	const char* componentLabel = widget->bindComponent.empty() ? "<Select Component>" : widget->bindComponent.c_str();
	if (ImGui::BeginCombo("Component", componentLabel))
	{
		for (Component* component : boundEntity ? boundEntity->Components() : std::vector<Component*>{})
		{
			if (!component)
			{
				continue;
			}
			if (component->GetBindableEvents().empty())
			{
				continue;
			}
			const bool selected = widget->bindComponent == component->Name();
			if (ImGui::Selectable(component->Name(), selected))
			{
				widget->bindComponent = component->Name();
				widget->bindMember.clear();
				widget->bindEvent.clear();
				SyncRuntimePreview();
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::EndDisabled();

	Component* boundComponent = boundEntity ? boundEntity->GetComponentByName(widget->bindComponent) : nullptr;
	ImGui::BeginDisabled(!boundComponent);
	const char* eventLabel = widget->bindEvent.empty() ? "<Select Event>" : widget->bindEvent.c_str();
	std::vector<BindableEvent> bindableEvents = boundComponent ? boundComponent->GetBindableEvents() : std::vector<BindableEvent>{};
	if (ImGui::BeginCombo("Event", eventLabel))
	{
		for (const BindableEvent& event : bindableEvents)
		{
			const bool selected = widget->bindEvent == event.name;
			const char* label = event.displayName.empty() ? event.name.c_str() : event.displayName.c_str();
			if (ImGui::Selectable(label, selected))
			{
				widget->bindEvent = event.name;
				SyncRuntimePreview();
			}
			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::EndDisabled();

	if (ImGui::Button("Close"))
	{
	m_bindingWidgetName.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}





