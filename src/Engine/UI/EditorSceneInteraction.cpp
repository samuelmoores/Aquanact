#include "Engine/UI/EditorSceneInteraction.h"
#include "Engine/UI/CameraPathCreator.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/LevelCollider.h"
#include "Engine/Core/PhysicsWorld.h"

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
	// UI controls must consume the click before the scene picker gets a chance to
	// clear the current entity selection. The state-machine button is submitted
	// in the Entity window, then scene interaction runs later in the same frame;
	// checking only the current hovered window is fragile when another window or
	// popup has become the active ImGui target.
	const ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureMouse
		|| ImGui::IsAnyItemActive()
		|| ImGui::IsAnyItemHovered();
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

bool EditorSceneInteraction::PickCameraPathPoint(
	const EngineGuiFrameContext& context, const glm::vec3& rayOrigin,
	const glm::vec3& rayDirection, int& pointIndex, float& distance)
{
	pointIndex = -1;
	distance = std::numeric_limits<float>::max();
	if (!context.cameraPath || !context.showCameraPath || !*context.showCameraPath) return false;
	const auto& points = context.cameraPath->Data().points;
	constexpr float markerRadius = 18.0f;
	for (std::size_t i = 0; i < points.size(); ++i)
	{
		const glm::vec3 offset = rayOrigin - points[i].position;
		const float halfB = glm::dot(offset, rayDirection);
		const float discriminant = halfB * halfB - (glm::dot(offset, offset) - markerRadius * markerRadius);
		if (discriminant < 0.0f) continue;
		const float root = std::sqrt(discriminant);
		float hitDistance = -halfB - root;
		if (hitDistance < 0.0f) hitDistance = -halfB + root;
		if (hitDistance >= 0.0f && hitDistance < distance)
		{
			distance = hitDistance;
			pointIndex = static_cast<int>(i);
		}
	}
	return pointIndex >= 0;
}

bool EditorSceneInteraction::PickLevelCollider(
	const EngineGuiFrameContext& context,
	const glm::vec3& rayOrigin,
	const glm::vec3& rayDirection,
	int& colliderIndex, float& distance)
{
	colliderIndex = -1;
	distance = std::numeric_limits<float>::max();
	if (!context.sceneManager || !context.sceneManager->ActiveLevel()) return false;
	const auto& colliders = context.sceneManager->ActiveLevel()->LevelColliders();
	for (std::size_t i = 0; i < colliders.size(); ++i)
	{
		const LevelCollider* collider = colliders[i].get();
		if (!collider) continue;
		glm::vec3 halfExtents = glm::abs(collider->Scale()) * 50.0f;
		if (collider->Shape() == LevelColliderShape::Capsule)
		{
			const glm::vec3 absoluteScale = glm::abs(collider->Scale());
			const float radius = collider->Radius() * std::max(absoluteScale.x, absoluteScale.z);
			halfExtents = glm::vec3(radius, collider->Height() * 0.5f * absoluteScale.y, radius);
		}
		else if (collider->Shape() == LevelColliderShape::Plane)
			halfExtents.y = 0.001f;
		const glm::vec3 minimum = collider->Position() - halfExtents;
		const glm::vec3 maximum = collider->Position() + halfExtents;
		float enter = 0.0f;
		float exit = std::numeric_limits<float>::max();
		bool hit = true;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::abs(rayDirection[axis]) <= 1e-6f)
			{
				hit = rayOrigin[axis] >= minimum[axis] && rayOrigin[axis] <= maximum[axis];
				if (!hit) break;
				continue;
			}
			float nearValue = (minimum[axis] - rayOrigin[axis]) / rayDirection[axis];
			float farValue = (maximum[axis] - rayOrigin[axis]) / rayDirection[axis];
			if (nearValue > farValue) std::swap(nearValue, farValue);
			enter = std::max(enter, nearValue);
			exit = std::min(exit, farValue);
			if (enter > exit) { hit = false; break; }
		}
		if (hit && enter >= 0.0f && enter < distance)
		{
			distance = enter;
			colliderIndex = static_cast<int>(i);
		}
	}
	return colliderIndex >= 0;
}

void EditorSceneInteraction::Draw(const EngineGuiFrameContext& context) const
{
	if (!context.camera || !context.sceneManager || !context.selection)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	if (m_boxFaceDrag.active)
	{
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !m_boxFaceDrag.collider)
		{
			m_boxFaceDrag.active = false;
		}
		else
		{
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			if (BuildWorldRay(*context.camera, io.MousePos, io.DisplaySize, rayOrigin, rayDirection))
			{
				const glm::vec3 axisVector = glm::vec3(
					m_boxFaceDrag.axis == 0 ? 1.0f : 0.0f,
					m_boxFaceDrag.axis == 1 ? 1.0f : 0.0f,
					m_boxFaceDrag.axis == 2 ? 1.0f : 0.0f);
				const float alignment = glm::dot(axisVector, rayDirection);
				const float denominator = 1.0f - alignment * alignment;
				if (denominator > 1e-6f)
				{
					const glm::vec3 offset = rayOrigin - m_boxFaceDrag.startPosition;
					const float rayProjection = glm::dot(rayDirection, offset);
					const float axisCoordinate = (glm::dot(axisVector, offset) - alignment * rayProjection) / denominator;
					const float worldDelta = (axisCoordinate - m_boxFaceDrag.startAxisCoordinate) * m_boxFaceDrag.direction;
					if (std::abs(worldDelta) <= 0.05f)
						return;
						glm::vec3 scale = m_boxFaceDrag.startScale;
						const float originalExtent = std::max(0.001f, std::abs(scale[m_boxFaceDrag.axis]) * 100.0f);
						const float newExtent = std::max(1.0f, originalExtent + worldDelta);
						scale[m_boxFaceDrag.axis] = newExtent / 100.0f;
						glm::vec3 position = m_boxFaceDrag.startPosition;
						position[m_boxFaceDrag.axis] += worldDelta * 0.5f * m_boxFaceDrag.direction;
						m_boxFaceDrag.collider->SetScale(scale);
						m_boxFaceDrag.collider->SetPosition(position);
						PhysicsWorld::Instance().Update(*m_boxFaceDrag.collider);
					}
				}
			}
			return;
		}
	Entity* selectedEntity = FindSelectedEntity(context);
	PointLight* selectedPointLight = nullptr;
	LevelCollider* selectedLevelCollider = nullptr;
	CameraPathPoint* selectedCameraPathPoint = nullptr;
	bool selectedCameraPathTrigger = false;
	std::vector<PointLight>& pointLights = Root::Current().Render().Lights().PointLights();
	if (context.selection->pointLightIndex >= 0 &&
		context.selection->pointLightIndex < static_cast<int>(pointLights.size()))
	{
		selectedPointLight = &pointLights[static_cast<std::size_t>(context.selection->pointLightIndex)];
	}
	if (context.selection->levelColliderIndex >= 0 && context.sceneManager->ActiveLevel())
	{
		auto& colliders = context.sceneManager->ActiveLevel()->LevelColliders();
		if (context.selection->levelColliderIndex < static_cast<int>(colliders.size()))
			selectedLevelCollider = colliders[static_cast<std::size_t>(context.selection->levelColliderIndex)].get();
	}
	if (context.cameraPath)
	{
		const int selectedPoint = context.cameraPath->SelectedPoint();
		auto& points = context.cameraPath->Data().points;
		if (selectedPoint >= 0 && selectedPoint < static_cast<int>(points.size()))
		{
			selectedCameraPathPoint = &points[static_cast<std::size_t>(selectedPoint)];
			selectedCameraPathTrigger = context.cameraPath->TriggerSelected() && selectedCameraPathPoint->island;
		}
	}
	if (selectedCameraPathPoint)
	{
		selectedEntity = nullptr;
		selectedPointLight = nullptr;
		selectedLevelCollider = nullptr;
	}
	Root::Current().Debugger().SetSelectedLevelCollider(selectedLevelCollider);
	const bool editorWindowCapturesMouse = IsEditorWindowCapturingMouse();
	const bool boxFaceDragMode = context.boxFaceDragMode && *context.boxFaceDragMode;
	Root::Current().Debugger().SetLevelColliderFaceEditMode(boxFaceDragMode);
	bool gizmoOwnsMouse = false;

#ifdef AQUANACT_EDITOR
	if (!boxFaceDragMode && (selectedEntity || selectedPointLight || selectedLevelCollider || selectedCameraPathPoint))
	{
		ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
		ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

		glm::mat4 gizmoMatrix = selectedEntity
			? selectedEntity->BuildModelMatrix()
			: selectedPointLight
			? glm::translate(glm::mat4(1.0f), selectedPointLight->position)
			: selectedLevelCollider
			? glm::translate(glm::mat4(1.0f), selectedLevelCollider->Position())
			: glm::translate(glm::mat4(1.0f), selectedCameraPathTrigger
				? selectedCameraPathPoint->triggerPosition
				: selectedCameraPathPoint->position);
		if (selectedLevelCollider)
		{
			gizmoMatrix = glm::translate(glm::mat4(1.0f), selectedLevelCollider->Position());
			gizmoMatrix = glm::rotate(gizmoMatrix, selectedLevelCollider->Rotation().z, glm::vec3(0.0f, 0.0f, 1.0f));
			gizmoMatrix = glm::rotate(gizmoMatrix, selectedLevelCollider->Rotation().y, glm::vec3(0.0f, 1.0f, 0.0f));
			gizmoMatrix = glm::rotate(gizmoMatrix, selectedLevelCollider->Rotation().x, glm::vec3(1.0f, 0.0f, 0.0f));
			gizmoMatrix = glm::scale(gizmoMatrix, selectedLevelCollider->Scale());
		}
		if (selectedEntity)
		{
			// Keep the gizmo at the visible mesh center while preserving the entity's
			// orientation and scale for the axis display.
			gizmoMatrix[3] = glm::vec4(selectedEntity->WorldCenterPosition(), 1.0f);
		}

		const bool wasUsingGizmo = ImGuizmo::IsUsing();
		// Keep the gizmo rendered and hittable while the cursor passes over editor
		// controls; active controls are still protected by the click-capture guard.
		ImGuizmo::Enable(true);
		const glm::mat4 viewMatrix = context.camera->GetViewMatrix();
		const glm::mat4 projectionMatrix = context.camera->GetProjectionMatrix();
		int gizmoOperationFlags = 0;
		if (!context.gizmoTranslate || *context.gizmoTranslate) gizmoOperationFlags |= ImGuizmo::TRANSLATE;
		if (!selectedCameraPathPoint && context.gizmoRotate && *context.gizmoRotate) gizmoOperationFlags |= ImGuizmo::ROTATE;
		if (!selectedCameraPathPoint && context.gizmoScale && *context.gizmoScale) gizmoOperationFlags |= ImGuizmo::SCALE;
		const bool gizmoEnabled = gizmoOperationFlags != 0;
		const bool manipulated = gizmoEnabled && ImGuizmo::Manipulate(
			glm::value_ptr(viewMatrix),
			glm::value_ptr(projectionMatrix),
			static_cast<ImGuizmo::OPERATION>(gizmoOperationFlags),
			ImGuizmo::WORLD,
			glm::value_ptr(gizmoMatrix));

		if (manipulated)
		{
			if (selectedEntity)
			{
				const glm::vec3 translation = glm::vec3(gizmoMatrix[3]);
				const glm::vec3 delta = translation - selectedEntity->WorldCenterPosition();
				selectedEntity->Translate(delta);
				if (context.gizmoRotate && *context.gizmoRotate || context.gizmoScale && *context.gizmoScale)
				{
					float componentsTranslation[3];
					float rotationDegrees[3];
					float scaleValues[3];
					ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(gizmoMatrix), componentsTranslation, rotationDegrees, scaleValues);
					if (context.gizmoRotate && *context.gizmoRotate)
						selectedEntity->SetRotation(glm::radians(glm::vec3(rotationDegrees[0], rotationDegrees[1], rotationDegrees[2])));
					if (context.gizmoScale && *context.gizmoScale)
						selectedEntity->SetScale(glm::vec3(scaleValues[0], scaleValues[1], scaleValues[2]));
				}
			}
			else if (selectedPointLight)
			{
				selectedPointLight->position = glm::vec3(gizmoMatrix[3]);
			}
			else if (selectedLevelCollider)
			{
				selectedLevelCollider->SetPosition(glm::vec3(gizmoMatrix[3]));
				if (context.gizmoRotate && *context.gizmoRotate || context.gizmoScale && *context.gizmoScale)
				{
					float translation[3];
					float rotationDegrees[3];
					float scaleValues[3];
					ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(gizmoMatrix), translation, rotationDegrees, scaleValues);
					if (context.gizmoRotate && *context.gizmoRotate)
						selectedLevelCollider->SetRotation(glm::radians(glm::vec3(rotationDegrees[0], rotationDegrees[1], rotationDegrees[2])));
					if (context.gizmoScale && *context.gizmoScale)
						selectedLevelCollider->SetScale(glm::vec3(scaleValues[0], scaleValues[1], scaleValues[2]));
				}
				PhysicsWorld::Instance().Update(*selectedLevelCollider);
			}
			else if (selectedCameraPathPoint)
			{
				if (selectedCameraPathTrigger)
					selectedCameraPathPoint->triggerPosition = glm::vec3(gizmoMatrix[3]);
				else
					selectedCameraPathPoint->position = glm::vec3(gizmoMatrix[3]);
			}
		}

		// The no-argument IsOver() reports the previous frame's hotspot in the
		// vendored ImGuizmo version. The operation-specific overload evaluates the
		// current translation handles and avoids discarding a new scene click.
		gizmoOwnsMouse = gizmoEnabled && (ImGuizmo::IsUsing() ||
			(ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
				((context.gizmoTranslate && *context.gizmoTranslate && ImGuizmo::IsOver(ImGuizmo::TRANSLATE)) ||
				 (context.gizmoRotate && *context.gizmoRotate && ImGuizmo::IsOver(ImGuizmo::ROTATE)) ||
				 (context.gizmoScale && *context.gizmoScale && ImGuizmo::IsOver(ImGuizmo::SCALE)))));
	}
#endif

	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
		editorWindowCapturesMouse || gizmoOwnsMouse)
		return;
	if (boxFaceDragMode)
	{
		// Face-drag mode is modal: only a box face can begin a drag, and no other
		// object type can take ownership of the click.
	}

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
	int pickedLevelColliderIndex = -1;
	float levelColliderDistance = std::numeric_limits<float>::max();
	PickLevelCollider(context, rayOrigin, rayDirection, pickedLevelColliderIndex, levelColliderDistance);
	int pickedCameraPointIndex = -1;
	float cameraPointDistance = std::numeric_limits<float>::max();
	PickCameraPathPoint(context, rayOrigin, rayDirection, pickedCameraPointIndex, cameraPointDistance);
	if (pickedLevelColliderIndex >= 0 && context.sceneManager->ActiveLevel())
	{
		LevelCollider* collider = context.sceneManager->ActiveLevel()->LevelColliders()[static_cast<std::size_t>(pickedLevelColliderIndex)].get();
		if (boxFaceDragMode && (!collider || collider->Shape() != LevelColliderShape::Box))
			return;
		if (collider && collider->Shape() == LevelColliderShape::Box)
		{
			const glm::vec3 halfExtents = glm::abs(collider->Scale()) * 50.0f;
			const glm::vec3 hitPoint = rayOrigin + rayDirection * levelColliderDistance;
			const glm::vec3 local = hitPoint - collider->Position();
			float bestGap = std::numeric_limits<float>::max();
			int faceAxis = -1;
			float faceDirection = 1.0f;
			for (int axis = 0; axis < 3; ++axis)
			{
				const float extent = halfExtents[axis];
				const float gapPositive = std::abs(local[axis] - extent);
				const float gapNegative = std::abs(local[axis] + extent);
				if (gapPositive < bestGap) { bestGap = gapPositive; faceAxis = axis; faceDirection = 1.0f; }
				if (gapNegative < bestGap) { bestGap = gapNegative; faceAxis = axis; faceDirection = -1.0f; }
			}
			if (faceAxis >= 0 && bestGap < 10.0f)
			{
				m_boxFaceDrag.active = true;
				m_boxFaceDrag.collider = collider;
				m_boxFaceDrag.axis = faceAxis;
				m_boxFaceDrag.direction = faceDirection;
				m_boxFaceDrag.startPosition = collider->Position();
				m_boxFaceDrag.startScale = collider->Scale();
				const glm::vec3 axisVector(
					faceAxis == 0 ? 1.0f : 0.0f,
					faceAxis == 1 ? 1.0f : 0.0f,
					faceAxis == 2 ? 1.0f : 0.0f);
				const float alignment = glm::dot(axisVector, rayDirection);
				const float denominator = 1.0f - alignment * alignment;
				if (denominator <= 1e-6f)
				{
					m_boxFaceDrag.active = false;
					return;
				}
				const glm::vec3 offset = rayOrigin - m_boxFaceDrag.startPosition;
				const float rayProjection = glm::dot(rayDirection, offset);
				m_boxFaceDrag.startAxisCoordinate =
					(glm::dot(axisVector, offset) - alignment * rayProjection) / denominator;
				Root::Current().Debugger().SetSelectedLevelColliderFace(faceAxis, faceDirection);
				context.selection->levelColliderIndex = pickedLevelColliderIndex;
				context.selection->entityId = 0;
				context.selection->pointLightIndex = -1;
				return;
			}
		}
	}

	if (pickedCameraPointIndex >= 0 && cameraPointDistance < entityDistance &&
		cameraPointDistance < pointLightDistance && cameraPointDistance < levelColliderDistance)
	{
		context.cameraPath->SelectPoint(pickedCameraPointIndex);
		context.selection->entityId = 0;
		context.selection->pointLightIndex = -1;
		context.selection->levelColliderIndex = -1;
	}
	else if (pickedLevelColliderIndex >= 0 && levelColliderDistance < entityDistance && levelColliderDistance < pointLightDistance)
	{
		if (context.cameraPath) context.cameraPath->SelectPoint(-1);
		context.selection->entityId = 0;
		context.selection->pointLightIndex = -1;
		context.selection->levelColliderIndex = pickedLevelColliderIndex;
	}
	else if (pickedPointLightIndex >= 0 && pointLightDistance < entityDistance)
	{
		if (context.cameraPath) context.cameraPath->SelectPoint(-1);
		context.selection->entityId = 0;
		context.selection->pointLightIndex = pickedPointLightIndex;
		context.selection->levelColliderIndex = -1;
	}
	else
	{
		if (context.cameraPath) context.cameraPath->SelectPoint(-1);
		context.selection->entityId = pickedEntityId;
		context.selection->pointLightIndex = -1;
		context.selection->levelColliderIndex = -1;
	}
}
