#include "Engine/UI/GameGUICreator.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"

#include <imgui.h>

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

void GameGUICreator::DrawCreateWidgetPopup()
{
	if (m_showCreateWidgetPopup)
	{
		ImGui::OpenPopup("Create Widget");
		m_showCreateWidgetPopup = false;
	}

	if (!ImGui::BeginPopupModal("Create Widget", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	const char* widgetKind = m_newWidgetIsProgressBar ? "Create a progress bar widget:" : (m_newWidgetIsImage ? "Create an image widget:" : "Create a button widget:");
	ImGui::TextUnformatted(widgetKind);
	ImGui::InputText("Name", m_newWidgetName, sizeof(m_newWidgetName));
	if (!m_newWidgetIsProgressBar)
	{
		ImGui::InputText("Texture", m_newWidgetTexture, sizeof(m_newWidgetTexture));
		ImGui::SameLine();
		if (ImGui::Button("Browse...##NewWidgetTexture"))
		{
			OpenTexturePicker(TexturePickerTarget::NewWidgetTexture);
		}
	}

	if (m_newWidgetIsImage || m_newWidgetIsProgressBar)
	{
		ImGui::TextUnformatted(m_newWidgetIsProgressBar ? "Configure the progress bar's binding:" : "Choose an image for the widget:");
	}

	if (m_newWidgetIsProgressBar)
	{
		Scene* activeLevel = Root::Current().Levels().ActiveLevel();
		ImGui::Separator();
		ImGui::TextUnformatted("Binding");

		GameGUIWidgetDef& preview = m_pendingProgressBarWidget;
		if (preview.name.empty())
		{
			preview.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "progress";
		}
		preview.texture = m_newWidgetTexture[0] != '\0' ? m_newWidgetTexture : "textures/example.png";
		preview.type = "ProgressBar";
		preview.layer = "Main";
		preview.width = 256;
		preview.height = 32;
		preview.textureWidth = 256;
		preview.textureHeight = 32;
		preview.defaultTextureWidth = 256;
		preview.defaultTextureHeight = 32;

		const char* entityLabel = preview.bindEntity.empty() ? "<Select Entity>" : preview.bindEntity.c_str();
		if (ImGui::BeginCombo("Entity", entityLabel))
		{
			if (activeLevel)
			{
				for (const auto& entity : activeLevel->Entities())
				{
					if (!entity)
					{
						continue;
					}
					const bool selected = preview.bindEntity == entity->Name();
					if (ImGui::Selectable(entity->Name().c_str(), selected))
					{
						preview.bindEntity = entity->Name();
						preview.bindComponent.clear();
						preview.bindMember.clear();
						preview.bindEvent.clear();
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
			}
			ImGui::EndCombo();
		}

		Entity* boundEntity = activeLevel ? FindEntity(activeLevel, preview.bindEntity) : nullptr;
		ImGui::BeginDisabled(!boundEntity);
		const char* componentLabel = preview.bindComponent.empty() ? "<Select Component>" : preview.bindComponent.c_str();
		if (ImGui::BeginCombo("Component", componentLabel))
		{
			for (Component* component : boundEntity ? boundEntity->Components() : std::vector<Component*>{})
			{
				if (!component || component->GetBindableMembers().empty())
				{
					continue;
				}
				const bool selected = preview.bindComponent == component->Name();
				if (ImGui::Selectable(component->Name(), selected))
				{
					preview.bindComponent = component->Name();
					preview.bindMember.clear();
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();

		Component* boundComponent = boundEntity ? boundEntity->GetComponentByName(preview.bindComponent) : nullptr;
		ImGui::BeginDisabled(!boundComponent);
		const char* memberLabel = preview.bindMember.empty() ? "<Select Value>" : preview.bindMember.c_str();
		std::vector<BindableMember> bindableMembers = boundComponent ? boundComponent->GetBindableMembers() : std::vector<BindableMember>{};
		if (ImGui::BeginCombo("Value", memberLabel))
		{
			for (const BindableMember& member : bindableMembers)
			{
				if (member.typeName != "int" && member.typeName != "float")
				{
					continue;
				}
				const bool selected = preview.bindMember == member.name;
				const char* label = member.displayName.empty() ? member.name.c_str() : member.displayName.c_str();
				if (ImGui::Selectable(label, selected))
				{
					preview.bindMember = member.name;
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
		ImGui::Separator();
		ImGui::TextUnformatted("Binding");
		GameGUIActionType action = m_newWidgetAction;
		const char* bindingLabel = action == GameGUIActionType::NewGame ? "New Game" : "None";
		if (ImGui::BeginCombo("Action", bindingLabel))
		{
			const GameGUIActionType options[] = { GameGUIActionType::None, GameGUIActionType::NewGame };
			for (GameGUIActionType option : options)
			{
				const bool selected = option == action;
				if (ImGui::Selectable(option == GameGUIActionType::NewGame ? "New Game" : "None", selected))
				{
					action = option;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		m_newWidgetAction = action;
		if (m_newWidgetAction == GameGUIActionType::NewGame)
		{
			SceneManager& sceneManager = Root::Current().Levels();
			const auto levelNames = sceneManager.SceneNames(SceneManager::SceneKind::Level);
			const char* launchLabel = m_newWidgetLaunchLevel.empty() ? "<Select Level>" : m_newWidgetLaunchLevel.c_str();
			if (ImGui::BeginCombo("Launch Level", launchLabel))
			{
				for (const std::string& levelName : levelNames)
				{
					const bool selected = m_newWidgetLaunchLevel == levelName;
					if (ImGui::Selectable(levelName.c_str(), selected))
					{
						m_newWidgetLaunchLevel = levelName;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			if (levelNames.empty())
			{
				ImGui::TextDisabled("No gameplay scenes are available.");
			}
		}
		else
		{
			m_newWidgetLaunchLevel.clear();
		}
	}

	if (ImGui::Button("Create"))
	{
		if (m_newWidgetIsProgressBar)
		{
			if (m_pendingProgressBarWidget.name.empty())
			{
				m_pendingProgressBarWidget.name = m_newWidgetName[0] != '\0' ? m_newWidgetName : "progress";
			}
			AddProgressBarWidget();
			m_pendingProgressBarWidget = {};
		}
		else if (m_newWidgetIsImage)
		{
			AddImageWidget();
		}
		else
		{
			AddButtonWidget();
		}
		SyncRuntimePreview();
		Root::Current().Debugger().LogMessage(m_newWidgetIsProgressBar ? "Create Progress Bar requested" : (m_newWidgetIsImage ? "Create Image requested" : "Create Button requested"));
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}





