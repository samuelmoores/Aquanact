#include "Engine/UI/SceneWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/LevelCollider.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <memory>
#include <string>
#include <vector>
#include <cctype>

namespace
{
	std::string NormalizeSceneName(const std::string& input)
	{
		std::string output;
		output.reserve(input.size());
		bool capitalizeNext = true;
		for (unsigned char ch : input)
		{
			if (std::isalnum(ch))
			{
				output.push_back(capitalizeNext ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch));
				capitalizeNext = false;
			}
			else
			{
				capitalizeNext = true;
			}
		}
		return output;
	}
}

void SceneWindow::DrawNewLevelPopup(SceneManager& sceneManager, bool& requested)
{
	if (requested)
	{
		m_newLevelStatusMessage.clear();
	}
	EngineGuiWidgets::ConsumePopupRequest("New Scene##AquanactNewLevel", requested);

	EngineGuiWidgets::ModalScope popup("New Scene##AquanactNewLevel");
	if (!popup)
		return;

	ImGui::TextUnformatted("Create a new scene:");
	ImGui::InputText("Name", m_newLevelName, sizeof(m_newLevelName));
	if (ImGui::Button("Create"))
	{
		const std::string levelName = NormalizeSceneName(m_newLevelName);
		if (levelName.empty())
			m_newLevelStatusMessage = "Enter a valid scene name.";
		else if (sceneManager.FindLevel(levelName))
			m_newLevelStatusMessage = "Scene already exists.";
		else if (sceneManager.CreateLevel(levelName))
		{
			sceneManager.SetActiveLevel(levelName);
			m_newLevelStatusMessage = "Created scene " + levelName + ".";
		}
		else
			m_newLevelStatusMessage = "Failed to create scene.";
	}
	ImGui::SameLine();
	EngineGuiWidgets::CloseButton();
	EngineGuiWidgets::StatusMessage(m_newLevelStatusMessage);
}

void SceneWindow::Draw(
	const EngineGuiFrameContext& context,
	bool& open,
	bool& entityWindowOpen) const
{
	if (!context.sceneManager || !context.selection)
		return;
	SceneManager& sceneManager = *context.sceneManager;
	unsigned int& selectedEntityId = context.selection->entityId;
	Scene* activeScene = sceneManager.ActiveLevel();
	const std::string title = activeScene ? activeScene->Name() : "Scene";
	if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		if (activeScene)
		{
			const std::vector<std::unique_ptr<Entity>>& objects = activeScene->Objects();
			for (std::size_t i = 0; i < objects.size(); ++i)
			{
				const std::unique_ptr<Entity>& object = objects[i];
				const std::string label = object ? object->Name() : "<null>";
				const std::string visibleLabel = label.empty() ? "<unnamed>" : label;
				const std::string selectableId = visibleLabel + "##LevelObject" + std::to_string(i);
				if (ImGui::Selectable(selectableId.c_str(), object && selectedEntityId == object->Id()))
				{
					selectedEntityId = object ? object->Id() : 0;
					context.selection->levelColliderIndex = -1;
					entityWindowOpen = true;
				}
			}
			if (objects.empty())
			{
				ImGui::TextUnformatted("No entities in this Scene.");
			}
		}
		else
		{
			ImGui::TextUnformatted("No active Scene selected.");
		}
	}
	ImGui::End();
}

void SceneWindow::DrawLevelColliderWindow(const EngineGuiFrameContext& context, bool& open) const
{
	if (!context.sceneManager || !context.selection) return;
	Scene* scene = context.sceneManager->ActiveLevel();
	if (!scene) return;
	if (!ImGui::Begin("Level Colliders", &open)) { ImGui::End(); return; }

	if (ImGui::Button("Add Level Collider"))
	{
		auto collider = std::make_unique<LevelCollider>("LevelCollider" + std::to_string(scene->LevelColliders().size() + 1));
		LevelCollider* added = scene->AddLevelCollider(std::move(collider));
		context.selection->levelColliderIndex = static_cast<int>(scene->LevelColliders().size() - 1);
		context.selection->entityId = 0;
		context.selection->pointLightIndex = -1;
		if (added) PhysicsWorld::Instance().Add(*added);
	}
	ImGui::TextUnformatted("Gizmo Operations");
	if (context.gizmoTranslate) ImGui::Checkbox("Translate", context.gizmoTranslate);
	if (context.gizmoRotate) { ImGui::SameLine(); ImGui::Checkbox("Rotate", context.gizmoRotate); }
	if (context.gizmoScale) { ImGui::SameLine(); ImGui::Checkbox("Scale", context.gizmoScale); }
	if (context.boxFaceDragMode) ImGui::Checkbox("Box Face Drag Mode", context.boxFaceDragMode);
	ImGui::SameLine();
	const int selected = context.selection->levelColliderIndex;
	if (ImGui::Button("Delete Selected") && selected >= 0 && selected < static_cast<int>(scene->LevelColliders().size()))
	{
		LevelCollider* removed = scene->LevelColliders()[static_cast<std::size_t>(selected)].get();
		PhysicsWorld::Instance().Remove(*removed);
		scene->RemoveLevelCollider(removed);
		context.selection->levelColliderIndex = -1;
	}

	ImGui::Separator();
	for (std::size_t i = 0; i < scene->LevelColliders().size(); ++i)
	{
		LevelCollider* collider = scene->LevelColliders()[i].get();
		if (collider && ImGui::Selectable((collider->Name() + "##ColliderWindow" + std::to_string(i)).c_str(), selected == static_cast<int>(i)))
		{
			context.selection->levelColliderIndex = static_cast<int>(i);
			context.selection->entityId = 0;
			context.selection->pointLightIndex = -1;
		}
	}
	if (selected >= 0 && selected < static_cast<int>(scene->LevelColliders().size()))
	{
		LevelCollider& collider = *scene->LevelColliders()[static_cast<std::size_t>(selected)];
		ImGui::Separator();
		ImGui::Text("Collider: %s", collider.Name().c_str());
		int shape = static_cast<int>(collider.Shape());
		const char* shapeNames[] = { "Box", "Capsule" };
		bool changed = false;
		if (ImGui::Combo("Shape", &shape, shapeNames, 2)) { collider.SetShape(static_cast<LevelColliderShape>(std::clamp(shape, 0, 1))); changed = true; }
		glm::vec3 position = collider.Position(), rotation = collider.Rotation(), scale = collider.Scale();
		if (ImGui::DragFloat3("Position", &position.x, 1.0f)) { collider.SetPosition(position); changed = true; }
		if (ImGui::DragFloat3("Rotation", &rotation.x, 0.01f)) { collider.SetRotation(rotation); changed = true; }
		if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.001f, 1000.0f)) { collider.SetScale(scale); changed = true; }
		if (collider.Shape() == LevelColliderShape::Capsule) { float radius = collider.Radius(); if (ImGui::DragFloat("Radius", &radius, 1.0f, 0.001f, 100000.0f)) { collider.SetRadius(radius); changed = true; } float height = collider.Height(); if (ImGui::DragFloat("Height", &height, 1.0f, radius * 2.0f, 100000.0f)) { collider.SetHeight(height); changed = true; } }
		bool visible = collider.DebugVisible(); if (ImGui::Checkbox("Debug Visible", &visible)) collider.SetDebugVisible(visible);
		if (changed) PhysicsWorld::Instance().Update(collider);
	}
	ImGui::End();
}
