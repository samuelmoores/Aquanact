#include "Engine/UI/EditorSceneInteraction.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"

#ifdef AQUANACT_EDITOR
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#endif

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

bool EditorSceneInteraction::BuildWorldRay(
	const Camera& camera,
	const ImVec2& mousePosition,
	const ImVec2& displaySize,
	glm::vec3& origin,
	glm::vec3& direction)
{
	if (displaySize.x <= 0.0f || displaySize.y <= 0.0f ||
		mousePosition.x < 0.0f || mousePosition.y < 0.0f ||
		mousePosition.x > displaySize.x || mousePosition.y > displaySize.y)
	{
		return false;
	}

	const float ndcX = (mousePosition.x / displaySize.x) * 2.0f - 1.0f;
	const float ndcY = 1.0f - (mousePosition.y / displaySize.y) * 2.0f;
	const glm::mat4 inverseViewProjection = glm::inverse(
		camera.GetProjectionMatrix() * camera.GetViewMatrix());
	const glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
	const glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
	if (std::abs(nearPoint.w) <= 1e-6f || std::abs(farPoint.w) <= 1e-6f)
		return false;

	origin = glm::vec3(nearPoint) / nearPoint.w;
	const glm::vec3 farWorld = glm::vec3(farPoint) / farPoint.w;
	const glm::vec3 ray = farWorld - origin;
	const float rayLength = glm::length(ray);
	if (!std::isfinite(rayLength) || rayLength <= 1e-6f)
		return false;

	direction = ray / rayLength;
	return true;
}

bool EditorSceneInteraction::IsEditorWindowCapturingMouse()
{
	// Capture this click only when it actually lands on an editor window. An
	// unrelated input widget may remain active until the user clicks the scene to
	// dismiss it; treating IsAnyItemActive() as mouse ownership discarded that
	// first scene click. Hovered items are already covered by their host window.
	// Avoid io.WantCaptureMouse here because ImGuizmo sets it for the next frame.
	return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
}

Entity* EditorSceneInteraction::FindSelectedEntity(const EngineGuiFrameContext& context)
{
	if (!context.sceneManager || !context.selection)
		return nullptr;
	Scene* activeScene = context.sceneManager->ActiveLevel();
	if (!activeScene || context.selection->entityId == 0)
		return nullptr;
	for (const auto& object : activeScene->Objects())
	{
		if (object && object->Id() == context.selection->entityId)
			return object.get();
	}
	return nullptr;
}

bool EditorSceneInteraction::PickEntity(
	const EngineGuiFrameContext& context,
	const glm::vec3& rayOrigin,
	const glm::vec3& rayDirection,
	unsigned int& entityId,
	float& distance)
{
	entityId = 0;
	distance = std::numeric_limits<float>::max();
	if (!context.sceneManager)
		return false;
	const Scene* activeScene = context.sceneManager->ActiveLevel();
	if (!activeScene)
		return false;

	Entity* closestEntity = nullptr;
	float closestDistance = std::numeric_limits<float>::max();
	for (const auto& object : activeScene->Objects())
	{
		if (!object || !object->GetMesh())
			continue;

		const glm::mat4 model = object->BuildModelMatrix();
		const glm::mat4 inverseModel = glm::inverse(model);
		const glm::vec3 localOrigin = glm::vec3(inverseModel * glm::vec4(rayOrigin, 1.0f));
		const glm::vec3 localDirection = glm::vec3(inverseModel * glm::vec4(rayDirection, 0.0f));
		if (glm::dot(localDirection, localDirection) <= 1e-12f)
			continue;

		float boundsDistance = 0.0f;
		if (!object->GetMesh()->RayHit(localOrigin, localDirection, boundsDistance))
			continue;

		float hitDistance = boundsDistance;
		// Static meshes use their actual triangles after the inexpensive bounds
		// broad phase. This prevents a large floor AABB from stealing clicks aimed
		// at smaller entities above it. Animated meshes retain bounds picking until
		// posed CPU vertex data is available.
		if (!object->skinned() && !object->GetMesh()->Faces().empty())
		{
			if (!object->GetMesh()->IntersectsTriangles(localOrigin, localDirection, hitDistance))
				continue;
		}

		if (hitDistance >= 0.0f && hitDistance < closestDistance)
		{
			closestDistance = hitDistance;
			closestEntity = object.get();
		}
	}

	entityId = closestEntity ? closestEntity->Id() : 0;
	distance = closestDistance;
	return closestEntity != nullptr;
}

bool EditorSceneInteraction::PickPointLight(
	const glm::vec3& rayOrigin,
	const glm::vec3& rayDirection,
	int& pointLightIndex,
	float& distance)
{
	pointLightIndex = -1;
	distance = std::numeric_limits<float>::max();
	if (!Root::Current().Debugger().ShowPointLightDebugSpheres())
		return false;

	const std::vector<PointLight>& pointLights = Root::Current().Render().Lights().PointLights();
	for (std::size_t i = 0; i < pointLights.size(); ++i)
	{
		const PointLight& pointLight = pointLights[i];
		const float markerRadius = std::clamp(pointLight.radius * 0.03f, 15.0f, 80.0f);
		const glm::vec3 originToCenter = rayOrigin - pointLight.position;
		const float halfB = glm::dot(originToCenter, rayDirection);
		const float c = glm::dot(originToCenter, originToCenter) - markerRadius * markerRadius;
		const float discriminant = halfB * halfB - c;
		if (discriminant < 0.0f)
			continue;

		const float root = std::sqrt(discriminant);
		float hitDistance = -halfB - root;
		if (hitDistance < 0.0f)
			hitDistance = -halfB + root;
		if (hitDistance < 0.0f || hitDistance >= distance)
			continue;

		distance = hitDistance;
		pointLightIndex = static_cast<int>(i);
	}

	return pointLightIndex >= 0;
}

void EditorSceneInteraction::Draw(const EngineGuiFrameContext& context) const
{
	if (!context.camera || !context.sceneManager || !context.selection)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	Entity* selectedEntity = FindSelectedEntity(context);
	PointLight* selectedPointLight = nullptr;
	std::vector<PointLight>& pointLights = Root::Current().Render().Lights().PointLights();
	if (context.selection->pointLightIndex >= 0 &&
		context.selection->pointLightIndex < static_cast<int>(pointLights.size()))
	{
		selectedPointLight = &pointLights[static_cast<std::size_t>(context.selection->pointLightIndex)];
	}
	const bool editorWindowCapturesMouse = IsEditorWindowCapturingMouse();
	bool gizmoOwnsMouse = false;

#ifdef AQUANACT_EDITOR
	if (selectedEntity || selectedPointLight)
	{
		ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
		ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

		glm::mat4 gizmoMatrix = selectedEntity
			? selectedEntity->BuildModelMatrix()
			: glm::translate(glm::mat4(1.0f), selectedPointLight->position);
		if (selectedEntity)
		{
			// Keep the gizmo at the visible mesh center while preserving the entity's
			// orientation and scale for the axis display.
			gizmoMatrix[3] = glm::vec4(selectedEntity->WorldCenterPosition(), 1.0f);
		}

		const bool wasUsingGizmo = ImGuizmo::IsUsing();
		ImGuizmo::Enable(!editorWindowCapturesMouse || wasUsingGizmo);
		const glm::mat4 viewMatrix = context.camera->GetViewMatrix();
		const glm::mat4 projectionMatrix = context.camera->GetProjectionMatrix();
		const bool manipulated = ImGuizmo::Manipulate(
			glm::value_ptr(viewMatrix),
			glm::value_ptr(projectionMatrix),
			ImGuizmo::TRANSLATE,
			ImGuizmo::WORLD,
			glm::value_ptr(gizmoMatrix));

		if (manipulated)
		{
			if (selectedEntity)
			{
				const glm::vec3 delta = glm::vec3(gizmoMatrix[3]) - selectedEntity->WorldCenterPosition();
				selectedEntity->Translate(delta);
			}
			else
			{
				selectedPointLight->position = glm::vec3(gizmoMatrix[3]);
			}
		}

		// The no-argument IsOver() reports the previous frame's hotspot in the
		// vendored ImGuizmo version. The operation-specific overload evaluates the
		// current translation handles and avoids discarding a new scene click.
		gizmoOwnsMouse = ImGuizmo::IsUsing() ||
			(ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
				ImGuizmo::IsOver(ImGuizmo::TRANSLATE));
	}
#endif

	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
		editorWindowCapturesMouse || gizmoOwnsMouse)
		return;

	glm::vec3 rayOrigin;
	glm::vec3 rayDirection;
	if (!BuildWorldRay(*context.camera, io.MousePos, io.DisplaySize, rayOrigin, rayDirection))
		return;

	unsigned int pickedEntityId = 0;
	float entityDistance = std::numeric_limits<float>::max();
	PickEntity(context, rayOrigin, rayDirection, pickedEntityId, entityDistance);

	int pickedPointLightIndex = -1;
	float pointLightDistance = std::numeric_limits<float>::max();
	PickPointLight(rayOrigin, rayDirection, pickedPointLightIndex, pointLightDistance);

	if (pickedPointLightIndex >= 0 && pointLightDistance < entityDistance)
	{
		context.selection->entityId = 0;
		context.selection->pointLightIndex = pickedPointLightIndex;
	}
	else
	{
		context.selection->entityId = pickedEntityId;
		context.selection->pointLightIndex = -1;
	}
}
