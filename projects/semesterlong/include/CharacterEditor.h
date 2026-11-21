#pragma once

#include "Engine/Core/Camera.h"
#include "Engine/Core/Component.h"

#include <glm/glm.hpp>

class Camera;
class Line;

class CharacterEditorPreviewCamera final : public Camera
{
public:
	void startUp() override {}
	void shutDown() override {}
	glm::mat4 GetProjectionMatrix() const override;
	glm::mat4 GetViewMatrix() const override { return m_view; }
	glm::vec3 GetPosition() const override { return m_position; }
	glm::vec3 GetFacing() const override { return m_facing; }
	void SetPose(const glm::vec3& position, const glm::vec3& facing, const glm::vec3& up);

private:
	glm::mat4 m_view{1.0f};
	glm::vec3 m_position{0.0f};
	glm::vec3 m_facing{0.0f, 0.0f, 1.0f};
};

class Entity;
class PlayerController;

class CharacterEditor final : public Component
{
public:
	#define CHARACTER_EDITOR_BINDABLES(VALUE, FUNCTION) \
		FUNCTION(IsOpen)
	AQUA_DECLARE_BINDABLES(CHARACTER_EDITOR_BINDABLES)
	#undef CHARACTER_EDITOR_BINDABLES

	~CharacterEditor() override;
	const char* Name() const override { return "CharacterEditor"; }
	void startUp(Entity& owner) override;
	void Update(Entity& owner, float dt) override;
	void DrawDebug(const Camera& camera) override;
	Camera* EditorPreviewCamera() override;
	Camera* RuntimeCameraOverride() override;
	bool GetEditorCameraPose(glm::vec3& position, glm::vec3& facing, glm::vec3& up) const override;

	void SetPlayerPositionOffset(const glm::vec2& offset) { m_playerPositionOffset = offset; }
	const glm::vec2& PlayerPositionOffset() const { return m_playerPositionOffset; }
	void SetCameraPosition(const glm::vec3& position) { m_cameraPosition = position; }
	const glm::vec3& CameraPosition() const { return m_cameraPosition; }
	void SetCameraRotation(const glm::vec3& rotationDegrees) { m_cameraRotationDegrees = rotationDegrees; }
	const glm::vec3& CameraRotation() const { return m_cameraRotationDegrees; }
	void SetShowPositionDebugSphere(bool show) { m_showPositionDebugSphere = show; }
	bool ShowPositionDebugSphere() const { return m_showPositionDebugSphere; }
	void SetShowCameraDebugSphere(bool show) { m_showCameraDebugSphere = show; }
	bool ShowCameraDebugSphere() const { return m_showCameraDebugSphere; }
	void SetPreviewEnabled(bool enabled) { m_previewEnabled = enabled; }
	bool PreviewEnabled() const { return m_previewEnabled; }
	void CaptureEngineCameraView();
	bool IsOpen() const { return m_open; }

private:
	void Enter(Entity& owner);
	void Leave();
	void ApplyEditorCamera();
	void FacePlayerToCamera(Entity& player);
	glm::vec3 DestinationPosition(Entity& player) const;

	PlayerController* m_playerController = nullptr;
	Entity* m_dresserTrigger = nullptr;
	Entity* m_player = nullptr;
	bool m_insideDresserTrigger = false;
	bool m_open = false;
	bool m_previousEPressed = false;
	bool m_previousMovementLock = false;

	glm::vec2 m_playerPositionOffset{0.0f, 160.0f};
	glm::vec3 m_cameraPosition{-2650.0f, 290.0f, 403.0f};
	glm::vec3 m_cameraRotationDegrees{0.0f, -90.0f, 0.0f};
	bool m_showPositionDebugSphere = false;
	bool m_showCameraDebugSphere = false;
	bool m_previewEnabled = false;
	Line* m_positionDebugSphere = nullptr;
	Line* m_cameraDebugSphere = nullptr;
	CharacterEditorPreviewCamera m_previewCamera;
};
