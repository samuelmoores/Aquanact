#pragma once

#include "Engine/UI/EngineGuiContext.h"
#include "Engine/Core/Camera.h"

#include <glm/glm.hpp>
#include <imgui.h>

class Entity;
class LevelCollider;
struct PointLight;

class EditorSceneInteraction
{
public:
	void Draw(const EngineGuiFrameContext& context) const;

private:
	struct BoxFaceDragState
	{
		bool active = false;
		class LevelCollider* collider = nullptr;
		int axis = -1;
		float direction = 1.0f;
		glm::vec3 startPosition{ 0.0f };
		glm::vec3 startScale{ 1.0f };
		float startAxisCoordinate = 0.0f;
		int hoveredAxis = -1;
		float hoveredDirection = 1.0f;
	};
	mutable BoxFaceDragState m_boxFaceDrag;
	static bool BuildWorldRay(const Camera& camera, const ImVec2& mousePosition,
		const ImVec2& displaySize, glm::vec3& origin, glm::vec3& direction);
	static bool IsEditorWindowCapturingMouse();
	static Entity* FindSelectedEntity(const EngineGuiFrameContext& context);
	static bool PickEntity(const EngineGuiFrameContext& context,
		const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
		unsigned int& entityId, float& distance);
	static bool PickPointLight(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
		int& pointLightIndex, float& distance);
	static bool PickCameraPathPoint(const EngineGuiFrameContext& context,
		const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
		int& pointIndex, float& distance);
	static bool PickLevelCollider(const EngineGuiFrameContext& context,
		const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
		int& colliderIndex, float& distance);
};
