#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Level.h"
#include "Engine/Core/LevelManager.h"

#include <imgui.h>
#include <vector>

namespace {
	Entity* FindEntity(Level* level, const std::string& name)
	{
		if (!level)
		{
			return nullptr;
		}
		for (const auto& entity : level->Entities())
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

	GameGUIAsset& asset = CurrentRoleGUI();
	GameGUIWidgetDef* widget = nullptr;
	GameGUIWidgetDef* draft = m_pendingProgressBarCreation ? &m_pendingProgressBarWidget : nullptr;
	if (draft)
	{
		widget = draft;
	}
	else
	{
		for (GameGUIWidgetDef& candidate : asset.widgets)
		{
			if (candidate.name == m_bindingWidgetName)
			{
				widget = &candidate;
				break;
			}
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

	Level* activeLevel = Root::Current().Levels().ActiveLevel();
	if (!activeLevel)
	{
		ImGui::TextDisabled("No active level is available.");
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
			if (widget->type == "ProgressBar")
			{
				if (component->GetBindableMembers().empty())
				{
					continue;
				}
			}
			else if (component->GetBindableEvents().empty())
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
	if (widget->type == "ProgressBar")
	{
		ImGui::BeginDisabled(!boundComponent);
		const char* memberLabel = widget->bindMember.empty() ? "<Select Member>" : widget->bindMember.c_str();
		std::vector<BindableMember> bindableMembers = boundComponent ? boundComponent->GetBindableMembers() : std::vector<BindableMember>{};
		if (ImGui::BeginCombo("Member", memberLabel))
		{
			for (const BindableMember& member : bindableMembers)
			{
				if (member.typeName != "int" && member.typeName != "float")
				{
					continue;
				}
				const bool selected = widget->bindMember == member.name;
				const char* label = member.displayName.empty() ? member.name.c_str() : member.displayName.c_str();
				if (ImGui::Selectable(label, selected))
				{
					widget->bindMember = member.name;
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
	}
	else
	{
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
	}

	if (ImGui::Button("Close"))
	{
		if (m_pendingProgressBarCreation)
		{
			m_pendingProgressBarBindingComplete = true;
			m_showTexturePickerPopup = true;
		}
		m_bindingWidgetName.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

