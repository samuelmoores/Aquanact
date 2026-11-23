#include "Engine/UI/EntityWindow.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/Component.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/TriggerSphere.h"
#include "Engine/Core/Hitbox.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SpawnManager.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/PhysicsWorld.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/ParticleSystem.h"
#include "Engine/UI/EngineGuiWidgets.h"
#include "Engine/UI/GameCodeMaintenance.h"
#include "AIController.h"
#include "Health.h"
#include "PlayerAttack.h"
#include "PlayerController.h"
#include "CharacterEditor.h"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cfloat>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
	std::string TriggerSphereClassName(Entity& entity)
	{
		std::string entityName = entity.Name();
		if (entity.IsSubmeshComponentOwner())
		{
			Entity* parent = entity.Parent();
			const Mesh* mesh = parent ? parent->GetMesh() : nullptr;
			const int submeshIndex = entity.SubmeshComponentIndex();
			if (mesh && submeshIndex >= 0 && submeshIndex < mesh->NumBuffers())
			{
				const std::string& meshName = mesh->SubMeshName(submeshIndex);
				if (!meshName.empty())
					entityName = meshName;
			}
			else if (const std::size_t separator = entityName.rfind(" / "); separator != std::string::npos)
			{
				entityName = entityName.substr(separator + 3);
			}
		}
		std::string suffix;
		suffix.reserve(entityName.size());
		bool capitalizeNext = true;
		for (unsigned char character : entityName)
		{
			if (std::isalnum(character))
			{
				suffix.push_back(capitalizeNext
					? static_cast<char>(std::toupper(character)) : static_cast<char>(character));
				capitalizeNext = false;
			}
			else
				capitalizeNext = true;
		}
		return "TriggerSphere" + (suffix.empty() ? std::string("Entity") : suffix);
	}
}

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
			Entity* componentOwner = &object;
			if (context.selection->submeshIndex >= 0)
				for (const auto& candidate : activeScene->Objects())
					if (candidate && candidate->Id() == context.selection->submeshComponentEntityId
						&& candidate->IsSubmeshComponentOwner()
						&& candidate->SubmeshComponentParentId() == object.Id()
						&& candidate->SubmeshComponentIndex() == context.selection->submeshIndex)
					{
						componentOwner = candidate.get();
						break;
					}
			const bool cutscene = sceneManager.SceneKindFor(activeScene->Name()) == SceneManager::SceneKind::Cutscene;
			if (!cutscene)
			{
				if (EntityStateMachine* stateMachine = componentOwner->GetEntityState())
				{
					result.stateMachineToDraw = stateMachine;
					if (DrawStateMachineButton())
						result.openStateMachine = true;
				}
			}

			const unsigned int deletedEntityId = object.Id();
			if (DrawDeletePopup(object) && activeScene->RemoveObject(&object))
			{
				for (std::size_t childIndex = activeScene->Objects().size(); childIndex > 0; --childIndex)
				{
					Entity* child = activeScene->Objects()[childIndex - 1].get();
					if (child && child->IsSubmeshComponentOwner()
						&& child->SubmeshComponentParentId() == deletedEntityId)
						activeScene->RemoveObject(child);
				}
				selectedEntityId = 0;
				context.selection->submeshIndex = -1;
				context.selection->submeshComponentEntityId = 0;
				result.entityDeleted = true;
			}
			else
			{
				ImGui::Separator();
				DrawTransform(object);
				DrawSelectedSubmesh(object, context.selection->submeshIndex);
				if (!cutscene)
				{
					DrawPhysics(object);
				}
				ImGui::SeparatorText("Components");
				const std::vector<Component*> components = componentOwner->Components();
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
					if (DrawRemoveComponentPopup(*componentOwner, *component, popupId.c_str()))
					{
						componentOwner->RemoveComponent(component);
						ImGui::PopID();
						continue;
					}
					ImGui::PopID();
				}
				DrawAddComponent(*componentOwner, cutscene, context);
				ImGui::Separator();
				if (ImGui::Button("Delete"))
					ImGui::OpenPopup("Delete Entity##Confirm");
			}
		}
	}
	ImGui::End();
	return result;
}

void EntityWindow::DrawSelectedSubmesh(Entity& entity, int selectedSubmeshIndex) const
{
	Mesh* mesh = entity.GetMesh();
	if (!mesh || selectedSubmeshIndex < 0 || selectedSubmeshIndex >= mesh->NumBuffers())
		return;
	ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
	if (!ImGui::CollapsingHeader("Submesh Settings"))
		return;

	ImGui::Text("Model: %s", entity.SourcePath().empty() ? "Generated mesh" : entity.SourcePath().c_str());
	const int selectedIndex = selectedSubmeshIndex;
	const SubMeshMaterial& material = mesh->GetMaterial(selectedIndex);
	if (m_materialPathMesh != mesh || m_materialPathSubmesh != selectedIndex)
	{
		m_materialPathMesh = mesh;
		m_materialPathSubmesh = selectedIndex;
		m_materialPathError = false;
		m_baseColorPathInput.fill('\0');
		m_roughnessPathInput.fill('\0');
		const std::string& basePath = mesh->ColorTextureOverridePath(selectedIndex);
		const std::string& roughnessPath = mesh->RoughnessTextureOverridePath(selectedIndex);
		std::memcpy(m_baseColorPathInput.data(), basePath.data(), std::min(basePath.size(), m_baseColorPathInput.size() - 1));
		std::memcpy(m_roughnessPathInput.data(), roughnessPath.data(), std::min(roughnessPath.size(), m_roughnessPathInput.size() - 1));
	}
	const std::string& sourceName = mesh->SubMeshName(selectedIndex);
	const std::string submeshLabel = sourceName.empty()
		? "Submesh " + std::to_string(selectedIndex) : sourceName;
	ImGui::Text("Submesh: %s", submeshLabel.c_str());
	ImGui::SeparatorText("Material");

	const auto drawTextureToggle = [&](const char* label, bool enabled, auto setEnabled)
	{
		bool value = enabled;
		if (ImGui::Checkbox(label, &value))
			setEnabled(value);
	};
	drawTextureToggle("Base Color", mesh->ColorTextureEnabled(selectedIndex),
		[mesh, selectedIndex](bool value) { mesh->SetColorTextureEnabled(selectedIndex, value); });
	ImGui::SameLine();
	drawTextureToggle("Roughness", mesh->RoughnessTextureEnabled(selectedIndex),
		[mesh, selectedIndex](bool value) { mesh->SetRoughnessTextureEnabled(selectedIndex, value); });
	if (!mesh->RoughnessTextureEnabled(selectedIndex) || !mesh->HasRoughnessTexture(selectedIndex))
	{
		float roughnessValue = mesh->RoughnessValue(selectedIndex);
		if (ImGui::SliderFloat("Roughness Value", &roughnessValue, 0.0f, 1.0f, "%.2f"))
			mesh->SetRoughnessValue(selectedIndex, roughnessValue);
	}
	ImGui::TextDisabled("Texture paths can be absolute or relative to the model folder (or its .fbm folder).");
	ImGui::InputText("Base Color Path", m_baseColorPathInput.data(), m_baseColorPathInput.size());
	if (ImGui::Button("Apply Base Color Path"))
		m_materialPathError = !mesh->SetColorTextureFile(selectedIndex, m_baseColorPathInput.data());
	ImGui::InputText("Roughness Path", m_roughnessPathInput.data(), m_roughnessPathInput.size());
	if (ImGui::Button("Apply Roughness Path"))
		m_materialPathError = !mesh->SetRoughnessTextureFile(selectedIndex, m_roughnessPathInput.data());
	if (m_materialPathError)
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Could not load the texture path.");

	ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
	if (ImGui::CollapsingHeader("Rendering"))
	{
		bool emissiveEnabled = material.emissiveEnabled;
		if (ImGui::Checkbox("Emissive", &emissiveEnabled))
			mesh->SetEmissiveEnabled(selectedIndex, emissiveEnabled);
		glm::vec3 emissiveColor = material.emissiveColor;
		if (ImGui::ColorEdit3("Emissive Color", &emissiveColor.x))
			mesh->SetEmissiveColor(selectedIndex, emissiveColor);
		float emissiveBlendFactor = material.emissiveBlendFactor;
		if (ImGui::SliderFloat("Emissive Blend", &emissiveBlendFactor, 0.0f, 1.0f, "%.2f"))
			mesh->SetEmissiveBlendFactor(selectedIndex, emissiveBlendFactor);
		bool castsShadow = mesh->CastsShadow(selectedIndex);
		if (ImGui::Checkbox("Casts Shadow", &castsShadow))
			mesh->SetCastsShadow(selectedIndex, castsShadow);
	}

	ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
	if (ImGui::CollapsingHeader("Submesh Collider"))
	{
		const std::size_t submeshIndex = static_cast<std::size_t>(selectedIndex);
		const SubmeshColliderSettings settings = entity.GetSubmeshColliderSettings(submeshIndex);
		bool collisionEnabled = settings.enabled;
		if (ImGui::Checkbox("Collider Enabled", &collisionEnabled))
		{
			entity.SetSubmeshColliderEnabled(submeshIndex, collisionEnabled);
			PhysicsWorld::Instance().Update(entity);
		}
		const char* colliderShapes[] = { "Box", "Capsule", "Convex" };
		int colliderShape = settings.shape == PhysicsColliderShape::Capsule ? 1
			: settings.shape == PhysicsColliderShape::Convex ? 2 : 0;
		if (entity.UseBaseMeshCollider())
			ImGui::BeginDisabled();
		if (!collisionEnabled)
			ImGui::BeginDisabled();
		if (ImGui::Combo("Collision Shape", &colliderShape, colliderShapes, IM_ARRAYSIZE(colliderShapes)))
		{
			entity.SetSubmeshColliderShape(submeshIndex, colliderShape == 1
				? PhysicsColliderShape::Capsule
				: colliderShape == 2 ? PhysicsColliderShape::Convex : PhysicsColliderShape::Box);
			PhysicsWorld::Instance().Update(entity);
		}
		if (!collisionEnabled)
			ImGui::EndDisabled();
		if (entity.UseBaseMeshCollider())
		{
			ImGui::EndDisabled();
			ImGui::TextDisabled("Inactive while Base Mesh Collision is enabled.");
		}
	}
}

void EntityWindow::DrawTransform(Entity& entity) const
{
	if (!ImGui::CollapsingHeader("Transform"))
		return;

	bool transformChanged = false;
	ImGui::TextUnformatted("Position");
	const glm::vec3 position = entity.Position();
	const glm::vec3 defaultPosition = entity.DefaultPosition();
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset##Position"))
	{
		entity.Translate(defaultPosition - position);
		transformChanged = true;
	}
	float x = position.x;
	float y = position.y;
	float z = position.z;
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("X##Position", &x, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
	{
		entity.Translate(glm::vec3(x - position.x, 0.0f, 0.0f));
		transformChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("Y##Position", &y, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
	{
		entity.Translate(glm::vec3(0.0f, y - position.y, 0.0f));
		transformChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("Z##Position", &z, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
	{
		entity.Translate(glm::vec3(0.0f, 0.0f, z - position.z));
		transformChanged = true;
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Rotation (degrees)");
	const glm::vec3 defaultRotation = entity.DefaultRotation();
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset##Rotation"))
	{
		entity.SetRotation(defaultRotation);
		transformChanged = true;
	}
	const glm::vec3 rotation = entity.Rotation();
	float rx = glm::degrees(rotation.x);
	float ry = glm::degrees(rotation.y);
	float rz = glm::degrees(rotation.z);
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("X##Rotation", &rx, 0.1f, -360.0f, 360.0f, "%.1f"))
	{
		entity.SetRotation(glm::vec3(glm::radians(rx), rotation.y, rotation.z));
		transformChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("Y##Rotation", &ry, 0.1f, -360.0f, 360.0f, "%.1f"))
	{
		entity.SetRotation(glm::vec3(rotation.x, glm::radians(ry), rotation.z));
		transformChanged = true;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(55.0f);
	if (ImGui::DragFloat("Z##Rotation", &rz, 0.1f, -360.0f, 360.0f, "%.1f"))
	{
		entity.SetRotation(glm::vec3(rotation.x, rotation.y, glm::radians(rz)));
		transformChanged = true;
	}
	if (transformChanged)
		PhysicsWorld::Instance().Update(entity);
}

void EntityWindow::DrawPhysics(Entity& entity) const
{
	if (!entity.GetMesh())
	{
		return;
	}
	if (!ImGui::CollapsingHeader("Physics"))
	{
		return;
	}

	bool rigidBody = entity.IsRigidBody();
	const bool hasController = entity.GetController() != nullptr;
	if (hasController)
		ImGui::BeginDisabled();
	if (ImGui::Checkbox("Rigid Body", &rigidBody))
	{
		entity.SetRigidBody(rigidBody);
		PhysicsWorld::Instance().Update(entity);
	}
	if (rigidBody)
	{
		float mass = entity.Mass();
		if (ImGui::DragFloat("Mass", &mass, 0.1f, 0.01f, 10000.0f, "%.2f"))
			entity.SetMass(mass);
		float linearDamping = entity.LinearDamping();
		if (ImGui::DragFloat("Linear Damping", &linearDamping, 0.01f, 0.0f, 100.0f, "%.2f"))
			entity.SetLinearDamping(linearDamping);
	}
	if (hasController)
	{
		ImGui::EndDisabled();
		ImGui::TextDisabled("Controller entities use controller movement and gravity.");
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
	bool useBaseMeshCollider = entity.UseBaseMeshCollider();
	if (ImGui::Checkbox("Base Mesh Collision", &useBaseMeshCollider))
	{
		entity.SetUseBaseMeshCollider(useBaseMeshCollider);
		PhysicsWorld::Instance().Update(entity);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Use one collider covering the combined bounds of every submesh. The entity Collision Shape setting applies.");
	float capsuleRadius = entity.CapsuleRadiusOverride();
	if (ImGui::DragFloat("Capsule Radius", &capsuleRadius, 0.5f, 0.0f, FLT_MAX, "%.2f"))
	{
		entity.SetCapsuleRadiusOverride(capsuleRadius);
		PhysicsWorld::Instance().Update(entity);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Radius in world units. Set to 0 to derive it automatically from the mesh bounds.");

	bool showBoundingBox = entity.ShowPhysicsBoundingBox();
	if (ImGui::Checkbox("Draw Bounding Volume", &showBoundingBox))
		entity.SetShowPhysicsBoundingBox(showBoundingBox);
	bool blocksCollision = entity.BlocksCollision();
	if (ImGui::Checkbox("Blocking Collision", &blocksCollision))
	{
		entity.SetBlocksCollision(blocksCollision);
		PhysicsWorld::Instance().Update(entity);
	}
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
	if (CharacterEditor* characterEditor = dynamic_cast<CharacterEditor*>(&component))
	{
		glm::vec2 playerOffset = characterEditor->PlayerPositionOffset();
		if (ImGui::DragFloat2("Player Position Offset (Dresser Local X/Z)", &playerOffset.x, 1.0f))
			characterEditor->SetPlayerPositionOffset(playerOffset);
		bool showPositionSphere = characterEditor->ShowPositionDebugSphere();
		if (ImGui::Checkbox("Draw Player Position Sphere", &showPositionSphere))
			characterEditor->SetShowPositionDebugSphere(showPositionSphere);

		ImGui::SeparatorText("Character Editor Camera");
		glm::vec3 cameraPosition = characterEditor->CameraPosition();
		if (ImGui::DragFloat3("Camera Position", &cameraPosition.x, 1.0f))
			characterEditor->SetCameraPosition(cameraPosition);
		glm::vec3 cameraRotation = characterEditor->CameraRotation();
		if (ImGui::DragFloat3("Camera Rotation (Degrees)", &cameraRotation.x, 0.5f))
			characterEditor->SetCameraRotation(cameraRotation);
		bool showCameraSphere = characterEditor->ShowCameraDebugSphere();
		if (ImGui::Checkbox("Draw Camera Position Sphere", &showCameraSphere))
			characterEditor->SetShowCameraDebugSphere(showCameraSphere);
		bool previewEnabled = characterEditor->PreviewEnabled();
		if (ImGui::Checkbox("Preview Camera View", &previewEnabled))
			characterEditor->SetPreviewEnabled(previewEnabled);
		if (ImGui::Button("Set From Engine Camera View"))
			characterEditor->CaptureEngineCameraView();
	}
	else if (ParticleSystem* particles = dynamic_cast<ParticleSystem*>(&component))
	{
		const ParticlePreset presets[] = {
			ParticlePreset::Custom,
			ParticlePreset::TorchFlame,
			ParticlePreset::Embers,
			ParticlePreset::BlueMagic,
			ParticlePreset::HealingMotes,
			ParticlePreset::PoisonGlow,
			ParticlePreset::ElectricalSparks,
			ParticlePreset::FireFountain,
			ParticlePreset::Smoke,
			ParticlePreset::Explosion,
			ParticlePreset::Trail
		};
		const char* presetNames[] = {
			"Custom",
			"Torch Flame",
			"Embers",
			"Magic Orb",
			"Healing Motes",
			"Poison Glow",
			"Electrical Sparks",
			"Fire Fountain",
			"Smoke",
			"Explosion",
			"Trail"
		};
		static_assert(IM_ARRAYSIZE(presets) == IM_ARRAYSIZE(presetNames));

		int presetIndex = 0;
		for (int index = 0; index < IM_ARRAYSIZE(presets); ++index)
		{
			if (presets[index] == particles->Preset())
			{
				presetIndex = index;
				break;
			}
		}
		if (ImGui::BeginCombo("Preset", presetNames[presetIndex]))
		{
			for (int index = 0; index < IM_ARRAYSIZE(presets); ++index)
			{
				const bool selected = index == presetIndex;
				if (ImGui::Selectable(presetNames[index], selected))
					particles->ApplyPreset(presets[index]);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::SmallButton("< Previous Preset"))
		{
			presetIndex = presetIndex == 0 ? IM_ARRAYSIZE(presets) - 1 : presetIndex - 1;
			particles->ApplyPreset(presets[presetIndex]);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Next Preset >"))
		{
			presetIndex = (presetIndex + 1) % IM_ARRAYSIZE(presets);
			particles->ApplyPreset(presets[presetIndex]);
		}

		float emissionRate = particles->EmissionRate();
		if (ImGui::DragFloat("Emission Rate", &emissionRate, 0.1f, 0.0f, 10000.0f)) particles->SetEmissionRate(emissionRate);
		int maxParticles = particles->MaxParticles();
		if (ImGui::InputInt("Max Particles", &maxParticles)) particles->SetMaxParticles(maxParticles);
		float lifetime = particles->Lifetime();
		if (ImGui::DragFloat("Lifetime", &lifetime, 0.01f, 0.01f, 60.0f)) particles->SetLifetime(lifetime);
		float size = particles->ParticleSize();
		if (ImGui::DragFloat("Start Size", &size, 0.01f, 0.001f, 100.0f)) particles->SetParticleSize(size);
		float endSize = particles->EndParticleSize();
		if (ImGui::DragFloat("End Size", &endSize, 0.01f, 0.0f, 100.0f)) particles->SetEndParticleSize(endSize);

		int emissionShape = static_cast<int>(particles->EmissionShape());
		const char* emissionShapes[] = {"Point", "Sphere", "Box"};
		if (ImGui::Combo("Emission Shape", &emissionShape, emissionShapes, IM_ARRAYSIZE(emissionShapes)))
			particles->SetEmissionShape(static_cast<ParticleEmissionShape>(emissionShape));
		if (particles->EmissionShape() == ParticleEmissionShape::Sphere)
		{
			float radius = particles->EmissionRadius();
			if (ImGui::DragFloat("Emission Radius", &radius, 0.05f, 0.0f, 1000.0f)) particles->SetEmissionRadius(radius);
		}
		else if (particles->EmissionShape() == ParticleEmissionShape::Box)
		{
			glm::vec3 extents = particles->EmissionBoxExtents();
			if (EngineGuiWidgets::Vector3Editor("Emission Extents", extents, 0.1f)) particles->SetEmissionBoxExtents(extents);
		}
		glm::vec3 velocity = particles->InitialVelocity();
		if (EngineGuiWidgets::Vector3Editor("Initial Velocity", velocity, 0.05f)) particles->SetInitialVelocity(velocity);
		float velocitySpread = particles->VelocitySpread();
		if (ImGui::DragFloat("Velocity Spread", &velocitySpread, 0.01f, 0.0f, 1000.0f)) particles->SetVelocitySpread(velocitySpread);
		float radialSpeed = particles->RadialSpeed();
		if (ImGui::DragFloat("Radial Speed", &radialSpeed, 0.05f, 0.0f, 1000.0f)) particles->SetRadialSpeed(radialSpeed);
		glm::vec3 gravity = particles->Gravity();
		if (EngineGuiWidgets::Vector3Editor("Gravity", gravity, 0.05f)) particles->SetGravity(gravity);
		glm::vec4 startColor = particles->StartColor();
		if (ImGui::ColorEdit4("Start Color", &startColor[0])) particles->SetStartColor(startColor);
		glm::vec4 endColor = particles->EndColor();
		if (ImGui::ColorEdit4("End Color", &endColor[0])) particles->SetEndColor(endColor);

		int blendMode = static_cast<int>(particles->BlendMode());
		const char* blendModes[] = {"Additive", "Alpha"};
		if (ImGui::Combo("Blend Mode", &blendMode, blendModes, IM_ARRAYSIZE(blendModes)))
			particles->SetBlendMode(static_cast<ParticleBlendMode>(blendMode));
		int simulationSpace = static_cast<int>(particles->SimulationSpace());
		const char* simulationSpaces[] = {"Local", "World"};
		if (ImGui::Combo("Simulation Space", &simulationSpace, simulationSpaces, IM_ARRAYSIZE(simulationSpaces)))
			particles->SetSimulationSpace(static_cast<ParticleSimulationSpace>(simulationSpace));
		bool looping = particles->Looping();
		if (ImGui::Checkbox("Looping", &looping)) particles->SetLooping(looping);
		if (ImGui::Button("Burst 20")) particles->Burst(20);
		ImGui::SameLine();
		if (ImGui::Button("Burst 64")) particles->Burst(64);
		ImGui::SameLine();
		if (ImGui::Button("Restart")) particles->Restart();
	}
	else if (PlayerAttack* attack = dynamic_cast<PlayerAttack*>(&component))
	{
		const auto& definitions = Root::Current().Spawns().Definitions();
		const std::string& currentName = attack->AttackInstanceName();
		if (ImGui::BeginCombo("Attack Instance", currentName.empty() ? "None" : currentName.c_str()))
		{
			if (ImGui::Selectable("None", currentName.empty()))
				attack->SetAttackInstanceName({});
			for (const auto& entry : definitions)
			{
				const bool selected = currentName == entry.first;
				if (ImGui::Selectable(entry.first.c_str(), selected))
					attack->SetAttackInstanceName(entry.first);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		float speed = attack->AttackInstanceSpeed();
		if (ImGui::DragFloat("Attack Instance Speed", &speed, 0.5f, 0.0f, 10000.0f, "%.1f"))
			attack->SetAttackInstanceSpeed(speed);
		int launchFrame = attack->AttackLaunchFrame();
		if (ImGui::InputInt("Attack Launch Frame", &launchFrame))
			attack->SetAttackLaunchFrame(launchFrame);
		glm::vec3 offset = attack->AttackInstanceOffset();
		if (EngineGuiWidgets::Vector3Editor("Attack Instance Offset", offset, 0.1f))
			attack->SetAttackInstanceOffset(offset);
		Entity* owner = attack->Owner();
		const std::vector<std::string> boneNames = owner && owner->GetMesh()
			? owner->GetMesh()->BoneNames() : std::vector<std::string>{};
		const char* bonePreview = attack->AttackInstanceBoneName().empty()
			? "Entity origin" : attack->AttackInstanceBoneName().c_str();
		if (ImGui::BeginCombo("Attack Instance Bone", bonePreview))
		{
			if (ImGui::Selectable("Entity origin", attack->AttackInstanceBoneName().empty()))
				attack->SetAttackInstanceBoneName({});
			for (const std::string& boneName : boneNames)
			{
				const bool selected = attack->AttackInstanceBoneName() == boneName;
				if (ImGui::Selectable(boneName.c_str(), selected))
					attack->SetAttackInstanceBoneName(boneName);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (boneNames.empty())
			ImGui::TextDisabled("No skeletal bones available; using entity origin.");
		bool drawAttackInstance = attack->DrawAttackInstance();
		if (ImGui::Checkbox("Draw Attack Instance", &drawAttackInstance))
			attack->SetDrawAttackInstance(drawAttackInstance);
		const auto& instanceDefinitions = Root::Current().Spawns().Definitions();
		const std::string& targetInstance = attack->AttackTargetInstanceName();
		if (ImGui::BeginCombo("Attack Target Instance", targetInstance.empty() ? "None" : targetInstance.c_str()))
		{
			if (ImGui::Selectable("None", targetInstance.empty()))
				attack->SetAttackTargetInstanceName({});
			for (const auto& entry : instanceDefinitions)
			{
				const bool selected = targetInstance == entry.first;
				if (ImGui::Selectable(entry.first.c_str(), selected))
					attack->SetAttackTargetInstanceName(entry.first);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (!attack->AttackTargetInstanceName().empty())
		{
			std::vector<std::string> boneNames{ "Entity origin" };
			if (Scene* scene = Root::Current().Scenes().ActiveLevel())
			{
				if (Entity* target = Root::Current().Spawns().FindActiveInstance(*scene, attack->AttackTargetInstanceName()))
					if (target->GetMesh())
					{
						const std::vector<std::string> targetBones = target->GetMesh()->BoneNames();
						boneNames.insert(boneNames.end(), targetBones.begin(), targetBones.end());
					}
			}
			int selectedBone = 0;
			for (std::size_t index = 1; index < boneNames.size(); ++index)
				if (boneNames[index] == attack->AttackTargetBoneName()) selectedBone = static_cast<int>(index);
			if (ImGui::Combo("Attack Target Bone", &selectedBone, [](void* data, int index, const char** outText)
			{
				const auto& values = *static_cast<const std::vector<std::string>*>(data);
				if (index < 0 || index >= static_cast<int>(values.size())) return false;
				*outText = values[static_cast<std::size_t>(index)].c_str();
				return true;
			}, &boneNames, static_cast<int>(boneNames.size())))
				attack->SetAttackTargetBoneName(selectedBone > 0 ? boneNames[static_cast<std::size_t>(selectedBone)] : std::string{});
			if (boneNames.size() == 1)
				ImGui::TextDisabled("Spawn the selected instance to choose a skeletal bone.");
		}
	}
	else if (PlayerController* player = dynamic_cast<PlayerController*>(&component))
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
	else if (AIController* ai = dynamic_cast<AIController*>(&component))
	{
		float detectionRange = ai->DetectionRange();
		if (ImGui::DragFloat("Detection Range", &detectionRange, 1.0f, 0.0f, 10000.0f)) ai->SetDetectionRange(detectionRange);
		float attackRange = ai->AttackRange();
		if (ImGui::DragFloat("Attack Range", &attackRange, 1.0f, 0.0f, 10000.0f)) ai->SetAttackRange(attackRange);
		float cooldown = ai->AttackCooldown();
		if (ImGui::DragFloat("Attack Cooldown", &cooldown, 0.01f, 0.0f, 60.0f)) ai->SetAttackCooldown(cooldown);
		float turnSpeed = ai->TurnSpeed();
		if (ImGui::DragFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 100.0f)) ai->SetTurnSpeed(turnSpeed);
		ImGui::Text("Target distance: %.1f", ai->TargetDistance());
	}
	else if (Controller* controller = dynamic_cast<Controller*>(&component))
	{
		float moveSpeed = controller->MoveSpeed();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f")) controller->SetMoveSpeed(moveSpeed);
	}
	else if (TriggerSphere* trigger = dynamic_cast<TriggerSphere*>(&component))
	{
		float radius = trigger->Radius();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.0f, 10000.0f, "%.2f")) trigger->SetRadius(radius);
		glm::vec3 positionOffset = trigger->PositionOffset();
		if (ImGui::DragFloat3("Position Offset", &positionOffset.x, 0.1f, -10000.0f, 10000.0f, "%.2f"))
			trigger->SetPositionOffset(positionOffset);
		bool enabled = trigger->Enabled();
		if (ImGui::Checkbox("Enabled", &enabled)) trigger->SetEnabled(enabled);
		bool debugDraw = Root::Current().Debugger().ShowTriggerSpheres();
		if (ImGui::Checkbox("Debug Draw", &debugDraw)) Root::Current().Debugger().SetShowTriggerSpheres(debugDraw);
	}
	else if (Hitbox* hitbox = dynamic_cast<Hitbox*>(&component))
	{
		bool drawEnabled = hitbox->DrawEnabled();
		if (ImGui::Checkbox("Draw Hitbox", &drawEnabled))
			hitbox->SetDrawEnabled(drawEnabled);

		const char* shapes[] = { "Box", "Sphere" };
		int shape = hitbox->Shape() == PhysicsColliderShape::Sphere ? 1 : 0;
		if (ImGui::Combo("Shape", &shape, shapes, IM_ARRAYSIZE(shapes)))
			hitbox->SetShape(shape == 1 ? PhysicsColliderShape::Sphere : PhysicsColliderShape::Box);

		float radius = hitbox->Radius();
		if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.001f, 10000.0f, "%.2f"))
			hitbox->SetRadius(radius);
		float damage = hitbox->Damage();
		if (ImGui::DragFloat("Damage", &damage, 0.5f, 0.0f, 100000.0f, "%.1f"))
			hitbox->SetDamage(damage);
		float activeStart = hitbox->ActiveStart();
		if (ImGui::SliderFloat("Active Start", &activeStart, 0.0f, 1.0f, "%.2f"))
			hitbox->SetActiveStart(activeStart);
		float activeEnd = hitbox->ActiveEnd();
		if (ImGui::SliderFloat("Active End", &activeEnd, 0.0f, 1.0f, "%.2f"))
			hitbox->SetActiveEnd(activeEnd);
		ImGui::Text("Currently Active: %s", hitbox->Active() ? "yes" : "no");

		Entity* owner = component.Owner();
		const std::vector<std::string> boneNames = owner && owner->GetMesh()
			? owner->GetMesh()->BoneNames() : std::vector<std::string>{};
		const char* bonePreview = hitbox->BoneName().empty() ? "Entity center" : hitbox->BoneName().c_str();
		if (ImGui::BeginCombo("Bone", bonePreview))
		{
			if (ImGui::Selectable("Entity center", hitbox->BoneName().empty()))
				hitbox->SetBoneName({});
			for (const std::string& boneName : boneNames)
			{
				const bool selected = hitbox->BoneName() == boneName;
				if (ImGui::Selectable(boneName.c_str(), selected))
					hitbox->SetBoneName(boneName);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (boneNames.empty())
			ImGui::TextDisabled("No skeletal bones available; using entity center.");
	}
	else if (Health* health = dynamic_cast<Health*>(&component))
	{
		float maxHealth = health->MaxHealth();
		if (ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 1.0f, 100000.0f, "%.1f"))
			health->SetMaxHealth(maxHealth);
		ImGui::Text("Current Health: %.1f", health->CurrentHealth());
		ImGui::Text("Dead: %s", health->IsDead() ? "yes" : "no");
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
		result.expanded = ImGui::CollapsingHeader(label.c_str());
		ImGui::TableSetColumnIndex(1);
		result.removeRequested = ImGui::SmallButton("Remove");
		ImGui::EndTable();
	}
	return result;
}

void EntityWindow::DrawAddComponent(
	Entity& entity, bool cutscene, const EngineGuiFrameContext& context) const
{
	ImGui::SetNextItemWidth(120.0f);
	if (!ImGui::BeginCombo("##AddComponent", "Add Component"))
	{
		DrawTriggerSphereCodePopup(context);
		return;
	}

	const std::vector<std::string> names = ComponentFactory::Instance().Names();
	std::vector<std::string> attached;
	for (Component* component : entity.Components())
		if (component)
		{
			attached.push_back(component->Name());
			if (dynamic_cast<TriggerSphere*>(component))
				attached.push_back("TriggerSphere");
		}

	for (const std::string& name : names)
	{
		const bool alreadyAttached = std::find(attached.begin(), attached.end(), name) != attached.end();
		const bool validAnimator = name != "EntityStateMachine"
			|| (entity.GetMesh() != nullptr && entity.GetMesh()->Skinned());
		ImGui::BeginDisabled(cutscene || alreadyAttached || !validAnimator);
		if (ImGui::Selectable(name.c_str()))
		{
			if (name == "TriggerSphere")
			{
				const std::string className = TriggerSphereClassName(entity);
				const std::vector<std::string> registeredNames = ComponentFactory::Instance().Names();
				const bool classIsLoaded = std::find(registeredNames.begin(), registeredNames.end(), className)
					!= registeredNames.end();
				if (classIsLoaded)
				{
					std::unique_ptr<Component> component = ComponentFactory::Instance().Create(className, entity);
					if (component) entity.AddComponent(std::move(component));
				}
				else
				{
					m_pendingTriggerClassName = className;
					m_triggerCodeStatusMessage.clear();
					m_pendingTriggerEntityId = entity.Id();
					m_openTriggerCodePopupRequested = true;
				}
			}
			else
			{
				std::unique_ptr<Component> component = ComponentFactory::Instance().Create(name, entity);
				if (component) entity.AddComponent(std::move(component));
			}
		}
		ImGui::EndDisabled();
	}
	if (std::find(attached.begin(), attached.end(), "TriggerSphere") != attached.end())
	{
		ImGui::Separator();
		ImGui::TextDisabled("A TriggerSphere is already attached to this entity.");
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
	if (m_openTriggerCodePopupRequested)
	{
		ImGui::OpenPopup("Generate Trigger C++ Class##ConfirmClose");
		m_openTriggerCodePopupRequested = false;
	}
	DrawTriggerSphereCodePopup(context);
}

void EntityWindow::DrawTriggerSphereCodePopup(const EngineGuiFrameContext& context) const
{
	if (!ImGui::BeginPopupModal("Generate Trigger C++ Class##ConfirmClose", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::TextWrapped("The engine needs to close so you can edit and build the new trigger class.");
	ImGui::TextWrapped("The generated class will be %s. The base TriggerSphere will be saved now; after editing its C++ file, rebuild the project and restart the editor.",
		m_pendingTriggerClassName.c_str());
	ImGui::Separator();
	if (ImGui::Button("Save and Close"))
	{
		bool ready = true;
		if (!context.projectManager || !context.sceneManager
			|| context.projectManager->CurrentProjectPath().empty())
		{
			m_triggerCodeStatusMessage = "Open a project before creating a custom trigger class.";
			ready = false;
		}
		if (ready)
		{
			const GameCodeGenerationResult result =
				GameCodeMaintenance::CreateTriggerSphereSubclass(m_pendingTriggerClassName);
			if (!result.success)
			{
				m_triggerCodeStatusMessage = result.statusMessage;
				ready = false;
			}
		}
		if (ready && !GameCodeMaintenance::SavePendingComponentAttachment(
			m_pendingTriggerClassName, m_pendingTriggerEntityId))
		{
			m_triggerCodeStatusMessage = "Could not save the pending trigger attachment.";
			ready = false;
		}
		Entity* pendingEntity = nullptr;
		Component* addedBaseTrigger = nullptr;
		if (ready && context.sceneManager && context.sceneManager->ActiveLevel())
		{
			for (const std::unique_ptr<Entity>& candidate : context.sceneManager->ActiveLevel()->Objects())
			{
				if (candidate && candidate->Id() == m_pendingTriggerEntityId)
				{
					pendingEntity = candidate.get();
					break;
				}
			}
			if (!pendingEntity)
			{
				m_triggerCodeStatusMessage = "Could not find the selected entity in the active scene.";
				ready = false;
			}
			else if (!pendingEntity->GetComponent<TriggerSphere>())
			{
				std::unique_ptr<Component> baseTrigger =
					ComponentFactory::Instance().Create("TriggerSphere", *pendingEntity);
				addedBaseTrigger = pendingEntity->AddComponent(std::move(baseTrigger));
				if (!addedBaseTrigger)
				{
					m_triggerCodeStatusMessage = "Could not add the TriggerSphere component.";
					ready = false;
				}
			}
		}
		if (ready && !context.projectManager->SaveProject(
			context.projectManager->CurrentProjectPath(), *context.sceneManager))
		{
			m_triggerCodeStatusMessage = "Could not save the project.";
			if (pendingEntity && addedBaseTrigger)
				pendingEntity->RemoveComponent(addedBaseTrigger);
			if (context.projectManager)
			{
				std::error_code removeError;
				std::filesystem::remove(context.projectManager->PendingClassConfigurationPath(), removeError);
			}
			ready = false;
		}
		else if (!ready)
		{
			if (pendingEntity && addedBaseTrigger)
				pendingEntity->RemoveComponent(addedBaseTrigger);
			if (context.projectManager)
			{
				std::error_code removeError;
				std::filesystem::remove(context.projectManager->PendingClassConfigurationPath(), removeError);
			}
		}
		if (ready)
		{
			Root::Current().Debugger().LogTagged("CodeGeneration",
				"Generated " + m_pendingTriggerClassName + ". Edit src/"
				+ m_pendingTriggerClassName + ".cpp, rebuild the project, then restart.");
			ImGui::CloseCurrentPopup();
			if (context.window)
				glfwSetWindowShouldClose(context.window->GLFW(), GLFW_TRUE);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		m_pendingTriggerClassName.clear();
		m_triggerCodeStatusMessage.clear();
		m_pendingTriggerEntityId = 0;
		ImGui::CloseCurrentPopup();
	}
	EngineGuiWidgets::StatusMessage(m_triggerCodeStatusMessage);
	ImGui::EndPopup();
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
