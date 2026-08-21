#pragma once

#include "Engine/UI/EngineGuiContext.h"
#include "Engine/Core/Camera.h"

#include <glm/glm.hpp>
#include <imgui.h>

class Entity;
struct PointLight;

class EditorSceneInteraction
{
public:
	void Draw(const EngineGuiFrameContext& context) const;

private:
	static bool BuildWorldRay(const Camera& camera, const ImVec2& mousePosition,
		const ImVec2& displaySize, glm::vec3& origin, glm::vec3& direction);
	static bool IsEditorWindowCapturingMouse();
	static Entity* FindSelectedEntity(const EngineGuiFrameContext& context);
	static bool PickEntity(const EngineGuiFrameContext& context,
		const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
		unsigned int& entityId, float& distance);
	static bool PickPointLight(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
		int& pointLightIndex, float& distance);
};
