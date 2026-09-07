#include "Engine/UI/EntityWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Component.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/TriggerSphere.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Game/Enemy.h"
#include "Game/PlayerController.h"

#include <imgui.h>
#include <cfloat>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

EntityWindowResult EntityWindow::Draw(const EngineGuiFrameContext& context, bool& open) const
{
	EntityWindowResult result;
	if (!context.sceneManager || !context.selection)
		return result;

	SceneManager& sceneManager = *context.sceneManager;
	Scene* activeScene = sceneManager.ActiveLevel();
	if (!activeScene)
		return result;

	const std::vector<std::unique_ptr<Entity>>& objects = activeScene->Objects();
	unsigned int& selectedEntityId = context.selection->entityId;
	std::size_t selectedIndex = objects.size();
	for (std::size_t index = 0; index < objects.size(); ++index)
	{
		if (objects[index] && objects[index]->Id() == selectedEntityId)
		{
			selectedIndex = index;
			break;
		}
	}
	if (selectedIndex == objects.size())
		selectedEntityId = 0;

	if (ImGui::Begin("Entity", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (selectedEntityId == 0)
		{
			ImGui::TextUnformatted("No entity selected.");
		}
		else if (!objects[selectedIndex])
		{
			ImGui::TextUnformatted("Selected object is null.");
		}
		else
		{
			Entity& object = *objects[selectedIndex];
			const bool cutscene = sceneManager.SceneKindFor(activeScene->Name()) == SceneManager::SceneKind::Cutscene;
			if (ImGui::Button("Delete"))
				ImGui::OpenPopup("Delete Entity##Confirm");
			ImGui::SameLine();
			DrawAddComponent(object, cutscene);

			if (DrawDeletePopup(object) && activeScene->RemoveObject(&object))
			{
				selectedEntityId = 0;
				result.entityDeleted = true;
			}
			else
			{
				DrawTransform(object);
				ImGui::Separator();
				if (!cutscene)
				{
					if (EntityStateMachine* stateMachine = object.GetEntityState())
					{
						result.stateMachineToDraw = stateMachine;
						if (DrawStateMachineButton())
							result.openStateMachine = true;
						ImGui::Separator();
					}
					DrawPhysics(object);
					ImGui::Separator();
				}
				ImGui::TextUnformatted("Components");
				ImGui::Separator();
				const std::vector<Component*> components = object.Components();
				for (std::size_t componentIndex = 0; componentIndex < components.size(); ++componentIndex)
				{
					Component* component = components[componentIndex];
					if (!component || dynamic_cast<EntityStateMachine*>(component)) continue;
					if (componentIndex > 0) ImGui::Separator();
					ImGui::PushID(component);
					const std::string popupId = "Remove Component##Confirm_" + std::to_string(reinterpret_cast<std::uintptr_t>(component));
					const EntityComponentHeaderResult header = DrawComponentHeader(*component);
					if (header.removeRequested) ImGui::OpenPopup(popupId.c_str());
					if (header.expanded) DrawComponentControls(*component);
					if (DrawRemoveComponentPopup(object, *component, popupId.c_str()))
					{
						object.RemoveComponent(component);
						ImGui::PopID();
						continue;
					}
					ImGui::PopID();
				}
			}
		}
	}
	ImGui::End();
	return result;
}

void EntityWindow::DrawTransform(Entity& entity) const
{
	ImGui::Separator();
	ImGui::TextUnformatted("Position");
	const glm::vec3 position = entity.Position();
	const glm::vec3 defaultPosition = entity.DefaultPosition();
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset##Position"))
	{
		entity.Translate(defaultPosition - position);
	}
	float x = position.x;
	float y = position.y;
	float z = position.z;
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("X##Position", &x, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
		entity.Translate(glm::vec3(x - position.x, 0.0f, 0.0f));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("Y##Position", &y, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
		entity.Translate(glm::vec3(0.0f, y - position.y, 0.0f));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("Z##Position", &z, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
		entity.Translate(glm::vec3(0.0f, 0.0f, z - position.z));

	ImGui::Separator();
	ImGui::TextUnformatted("Rotation");
	const glm::vec3 defaultRotation = entity.DefaultRotation();
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset##Rotation"))
		entity.SetRotation(defaultRotation);
	const glm::vec3 rotation = entity.Rotation();
	float rx = rotation.x;
	float ry = rotation.y;
	float rz = rotation.z;
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("X##Rotation", &rx, 0.1f, -360.0f, 360.0f, "%.1f"))
		entity.SetRotation(glm::vec3(rx, rotation.y, rotation.z));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("Y##Rotation", &ry, 0.1f, -360.0f, 360.0f, "%.1f"))
		entity.SetRotation(glm::vec3(rotation.x, ry, rotation.z));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("Z##Rotation", &rz, 0.1f, -360.0f, 360.0f, "%.1f"))
		entity.SetRotation(glm::vec3(rotation.x, rotation.y, rz));
}

void EntityWindow::DrawPhysics(Entity& entity) const
{
	if (!entity.GetMesh())
	{
		return;
	}
	if (!ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	const char* colliderShapes[] = { "Box", "Capsule", "Convex" };
	int colliderShape = entity.GetPhysicsColliderShape() == PhysicsColliderShape::Capsule ? 1
		: entity.GetPhysicsColliderShape() == PhysicsColliderShape::Convex ? 2 : 0;
	if (ImGui::Combo("Collision Shape", &colliderShape, colliderShapes, IM_ARRAYSIZE(colliderShapes)))
	{
		entity.SetPhysicsColliderShape(colliderShape == 1
			? PhysicsColliderShape::Capsule
			: colliderShape == 2 ? PhysicsColliderShape::Convex : PhysicsColliderShape::Box);
		PhysicsWorld::Instance().Update(entity);
	}

	bool showBoundingBox = entity.ShowPhysicsBoundingBox();
	if (ImGui::Checkbox("Draw Bounding Volume", &showBoundingBox))
		entity.SetShowPhysicsBoundingBox(showBoundingBox);
	bool ignoreCameraCollision = entity.IgnoreCameraCollision();
	if (ImGui::Checkbox("Ignore Camera Collision", &ignoreCameraCollision))
		entity.SetIgnoreCameraCollision(ignoreCameraCollision);
	bool blocksCameraView = entity.BlocksCameraView();
	if (ImGui::Checkbox("Blocks Camera View", &blocksCameraView))
		entity.SetBlocksCameraView(blocksCameraView);
}

bool EntityWindow::DrawStateMachineButton() const
{
	ImGui::Separator();
	return ImGui::Button("Entity State Machine");
}

void EntityWindow::DrawComponentControls(Component& component) const
{
	if (PlayerController* player = dynamic_cast<PlayerController*>(&component))
	{
		float moveSpeed = player->MoveSpeed();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f")) player->SetMoveSpeed(moveSpeed);
		float turnSpeed = player->TurnSpeed();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputFloat("Turn Speed", &turnSpeed, 0.0f, 0.0f, "%.2f")) player->SetTurnSpeed(turnSpeed);
		float slope = player->MaxSlopeAngle();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::DragFloat("Max Slope Angle", &slope, 0.5f, 0.0f, 89.0f, "%.1f degrees")) player->SetMaxSlopeAngle(slope);
	}
	else if (Controller* controller = dynamic_cast<Controller*>(&component))
	{
		float moveSpeed = controller->MoveSpeed();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f")) controller->SetMoveSpeed(moveSpeed);
	}
	else if (dynamic_cast<Enemy*>(&component))
	{
		ImGui::TextUnformatted("Enemy behavior component");
	}
	else if (TriggerSphere* trigger = dynamic_cast<TriggerSphere*>(&component))
	{
		float radius = trigger->Radius();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.0f, 10000.0f, "%.2f")) trigger->SetRadius(radius);
		bool enabled = trigger->Enabled();
		if (ImGui::Checkbox("Enabled", &enabled)) trigger->SetEnabled(enabled);
		bool debugDraw = Root::Current().Debugger().ShowTriggerSpheres();
		if (ImGui::Checkbox("Debug Draw", &debugDraw)) Root::Current().Debugger().SetShowTriggerSpheres(debugDraw);
	}
	else
	{
		ImGui::TextUnformatted("No editor controls for this component.");
	}
}

EntityComponentHeaderResult EntityWindow::DrawComponentHeader(Component& component) const
{
	EntityComponentHeaderResult result;
	const std::string label = component.Name();
	const std::string componentHeaderId =
		std::string("ComponentHeader##") + std::to_string(reinterpret_cast<std::uintptr_t>(&component));
	const float removeButtonWidth =
		ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;

	if (ImGui::BeginTable(componentHeaderId.c_str(), 2,
		ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, removeButtonWidth);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		result.expanded = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
		ImGui::TableSetColumnIndex(1);
		result.removeRequested = ImGui::SmallButton("Remove");
		ImGui::EndTable();
	}
	return result;
}

void EntityWindow::DrawAddComponent(Entity& entity, bool cutscene) const
{
	ImGui::SetNextItemWidth(120.0f);
	if (!ImGui::BeginCombo("##AddComponent", "Add Component"))
		return;

	const std::vector<std::string> names = ComponentFactory::Instance().Names();
	std::vector<std::string> attached;
	for (Component* component : entity.Components())
		if (component) attached.push_back(component->Name());

	for (const std::string& name : names)
	{
		const bool alreadyAttached = std::find(attached.begin(), attached.end(), name) != attached.end();
		const bool validAnimator = name != "EntityStateMachine"
			|| (entity.GetMesh() != nullptr && entity.GetMesh()->Skinned());
		ImGui::BeginDisabled(cutscene || alreadyAttached || !validAnimator);
		if (ImGui::Selectable(name.c_str()))
		{
			std::unique_ptr<Component> component = ComponentFactory::Instance().Create(name, entity);
			if (component) entity.AddComponent(std::move(component));
		}
		ImGui::EndDisabled();
	}
	if (cutscene)
	{
		ImGui::Separator();
		ImGui::TextDisabled("Cutscenes cannot receive gameplay components.");
	}
	else if (names.empty())
	{
		ImGui::Separator();
		ImGui::TextDisabled("No component types are registered.");
	}
	else if (attached.size() >= names.size())
	{
		ImGui::Separator();
		ImGui::TextDisabled("All components are already attached.");
	}
	ImGui::EndCombo();
}

bool EntityWindow::DrawDeletePopup(const Entity& entity) const
{
	bool confirmed = false;
	if (ImGui::BeginPopupModal("Delete Entity##Confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Delete %s from the scene?", entity.Name().empty() ? "<unnamed>" : entity.Name().c_str());
		ImGui::TextDisabled("This removes the entity from the active Scene.");
		if (ImGui::Button("Delete"))
		{
			confirmed = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	return confirmed;
}

bool EntityWindow::DrawRemoveComponentPopup(
	const Entity& entity, const Component& component, const char* popupId) const
{
	bool confirmed = false;
	if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Remove %s from %s?", component.Name(), entity.Name().c_str());
		ImGui::TextDisabled("This change is permanent after the project is saved.");
		if (ImGui::Button("Remove"))
		{
			confirmed = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	return confirmed;
}
