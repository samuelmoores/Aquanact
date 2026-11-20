#include "Engine/UI/SceneWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/LevelCollider.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/EngineGuiWidgets.h"

#include <imgui.h>

#include <memory>
#include <string>
#include <vector>
#include <cctype>

namespace
{
	Entity* FindSubmeshComponentEntity(Scene& scene, unsigned int parentId, int submeshIndex)
	{
		for (const auto& object : scene.Objects())
			if (object && object->IsSubmeshComponentOwner()
				&& object->SubmeshComponentParentId() == parentId
				&& object->SubmeshComponentIndex() == submeshIndex)
				return object.get();
		return nullptr;
	}

	Entity* EnsureSubmeshComponentEntity(Scene& scene, Entity& parent, int submeshIndex)
	{
		if (Entity* existing = FindSubmeshComponentEntity(scene, parent.Id(), submeshIndex))
			return existing;
		Mesh* mesh = parent.GetMesh();
		if (!mesh || submeshIndex < 0 || submeshIndex >= mesh->NumBuffers())
			return nullptr;

		const glm::vec3 localCenter = (mesh->SubMeshMinBounds(submeshIndex)
			+ mesh->SubMeshMaxBounds(submeshIndex)) * 0.5f;
		const std::string& sourceName = mesh->SubMeshName(submeshIndex);
		const std::string childName = parent.Name() + " / "
			+ (sourceName.empty() ? "Submesh " + std::to_string(submeshIndex) : sourceName);
		auto child = std::make_unique<Entity>(childName, false);
		child->SetSubmeshComponentOwner(parent.Id(), submeshIndex);
		child->AttachTo(parent, localCenter);
		return scene.AddObject(std::move(child));
	}

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
	const char* sceneKinds[] = { "Level", "Cutscene" };
	ImGui::Combo("Type", &m_newSceneKind, sceneKinds, 2);
	ImGui::InputText("Name", m_newLevelName, sizeof(m_newLevelName));
	if (ImGui::Button("Create"))
	{
		const std::string levelName = NormalizeSceneName(m_newLevelName);
		if (levelName.empty())
			m_newLevelStatusMessage = "Enter a valid scene name.";
		else if (sceneManager.FindLevel(levelName))
			m_newLevelStatusMessage = "Scene already exists.";
		else if ((m_newSceneKind == 0 ? sceneManager.CreateLevel(levelName) : sceneManager.CreateCutscene(levelName)))
		{
			// Persist the currently edited GUI before activating the new scene.
			// Otherwise a New Game destination can appear to revert after the
			// scene editor changes scenes.
			Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
			sceneManager.SetActiveLevel(levelName);
			m_newLevelStatusMessage = "Created " + std::string(sceneKinds[m_newSceneKind]) + " " + levelName + ".";
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
	unsigned int pendingSubmeshParentId = 0;
	int pendingSubmeshIndex = -1;
	Scene* activeScene = sceneManager.ActiveLevel();
	const std::string title = activeScene ? activeScene->Name() : "Scene";
	if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		if (activeScene)
		{
			const std::vector<std::unique_ptr<Entity>>& objects = activeScene->Objects();
			ImGui::Separator();
			bool hasSceneObject = false;
			for (std::size_t i = 0; i < objects.size(); ++i)
			{
				const std::unique_ptr<Entity>& object = objects[i];
				if (object && object->IsSubmeshComponentOwner()) continue;
				hasSceneObject = true;
				const std::string label = object ? object->Name() : "<null>";
				const std::string visibleLabel = label.empty() ? "<unnamed>" : label;
				const bool hasSubmeshes = object && object->GetMesh() && object->GetMesh()->NumBuffers() > 0;
				ImGui::PushID(static_cast<int>(i));
				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth
					| ImGuiTreeNodeFlags_OpenOnArrow
					| ((object && selectedEntityId == object->Id() && context.selection->submeshIndex < 0)
						? ImGuiTreeNodeFlags_Selected : 0)
					| (hasSubmeshes ? 0 : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
				const bool expanded = ImGui::TreeNodeEx(visibleLabel.c_str(), flags);
				if (ImGui::IsItemClicked())
				{
					selectedEntityId = object ? object->Id() : 0;
					context.selection->submeshIndex = -1;
					context.selection->submeshComponentEntityId = 0;
					context.selection->levelColliderIndex = -1;
					context.selection->pointLightIndex = -1;
					entityWindowOpen = true;
				}
				if (expanded && hasSubmeshes)
				{
					Mesh* mesh = object->GetMesh();
					for (int submeshIndex = 0; submeshIndex < mesh->NumBuffers(); ++submeshIndex)
					{
						const std::string& sourceName = mesh->SubMeshName(submeshIndex);
						const std::string childLabel = sourceName.empty()
							? "Submesh " + std::to_string(submeshIndex)
							: sourceName + " (Submesh " + std::to_string(submeshIndex) + ")";
						const bool selected = object->Id() == selectedEntityId
							&& context.selection->submeshIndex == submeshIndex;
						ImGui::PushID(submeshIndex);
						if (ImGui::Selectable(childLabel.c_str(), selected))
						{
							selectedEntityId = object->Id();
							context.selection->submeshIndex = submeshIndex;
							context.selection->submeshComponentEntityId = 0;
							pendingSubmeshParentId = object->Id();
							pendingSubmeshIndex = submeshIndex;
							context.selection->levelColliderIndex = -1;
							context.selection->pointLightIndex = -1;
							entityWindowOpen = true;
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			if (!hasSceneObject)
			{
				ImGui::TextUnformatted("No entities in this Scene.");
			}
			if (pendingSubmeshParentId != 0 && pendingSubmeshIndex >= 0)
			{
				Entity* pendingParent = nullptr;
				for (const auto& object : activeScene->Objects())
					if (object && object->Id() == pendingSubmeshParentId)
					{
						pendingParent = object.get();
						break;
					}
				if (pendingParent)
					if (Entity* componentEntity = EnsureSubmeshComponentEntity(
						*activeScene, *pendingParent, pendingSubmeshIndex))
						context.selection->submeshComponentEntityId = componentEntity->Id();
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
		context.selection->submeshIndex = -1;
		context.selection->submeshComponentEntityId = 0;
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
			context.selection->submeshIndex = -1;
			context.selection->submeshComponentEntityId = 0;
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
