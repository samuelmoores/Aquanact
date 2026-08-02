#pragma once

#include "Engine/Core/Camera.h"
#include "Engine/Core/EngineCamera.h"
#include "GLFW/glfw3.h"

#include <string>
#include <memory>

class Entity;
class CameraCollider;

class GameCamera final : public Camera {
public:
	GameCamera();
	~GameCamera() override;
	void startUp() override;
	void shutDown() override;
	glm::mat4 GetProjectionMatrix() const override;
	glm::mat4 GetViewMatrix() const override;
	glm::vec3 GetPosition() const override;
	glm::vec3 GetFacing() const override;
	void CopyFrom(const EngineCamera& camera);
	void SetPose(const glm::vec3& position, const glm::vec3& facing);
	void CaptureEditorState();
	void RestoreEditorState();
	void SetTarget(Entity* target);
	Entity* Target() const { return m_target; }
	unsigned int TargetId() const { return m_targetId; }
	void SetTargetName(std::string targetName) { m_targetName = std::move(targetName); }
	const std::string& TargetName() const { return m_targetName; }
	float Radius() const { return m_radius; }
	void SetRadius(float radius);
	float Yaw() const { return m_yaw; }
	float Pitch() const { return m_pitch; }
	void SetOrbitAngles(float yaw, float pitch);
	void UpdateThirdPerson(const class Input& input, float dt);
	float ColliderRadius() const;
	void SetColliderRadius(float radius);
	CameraCollider& Collider();
	const CameraCollider& Collider() const;

private:
	void RebuildView();

	float m_fieldOfView = 45.0f;
	float m_nearPlane = 0.1f;
	float m_farPlane = 1000000.0f;
	glm::mat4 m_projection_matrix{ 1.0f };
	glm::mat4 m_view_matrix{ 1.0f };
	glm::vec3 m_position{ 0.0f, 0.0f, -10.0f };
	glm::vec3 m_front{ 0.0f, 0.0f, 1.0f };
	glm::vec3 m_up{ 0.0f, 1.0f, 0.0f };
	GLFWwindow* m_window = nullptr;
	Entity* m_target = nullptr;
	unsigned int m_targetId = 0;
	std::string m_targetName;
	float m_radius = 10.0f;
	float m_yaw = 0.0f;
	float m_pitch = 15.0f;
	float m_lookSensitivity = 1.2f;
	glm::vec3 m_editorPosition{ 0.0f, 0.0f, -10.0f };
	glm::vec3 m_editorFacing{ 0.0f, 0.0f, 1.0f };
	float m_editorRadius = 10.0f;
	float m_editorYaw = 0.0f;
	float m_editorPitch = 15.0f;
	float m_editorColliderRadius = 25.0f;
	bool m_hasEditorState = false;
	std::unique_ptr<CameraCollider> m_collider;
	glm::vec3 m_lastSafePosition{ 0.0f, 0.0f, -10.0f };
	bool m_hasSafePosition = false;
};


