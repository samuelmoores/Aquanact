#pragma once

#include "Engine/Camera.h"
#include "GLFW/glfw3.h"

class EngineCamera final : public Camera {
public:
	EngineCamera() = default;
	void startUp() override;
	void shutDown() override;
	glm::mat4 GetProjectionMatrix() const override;
	glm::mat4 GetViewMatrix() const override;
	glm::vec3 GetPosition() const override;
	glm::vec3 GetFacing() const override;
	void FlyControl(glm::vec2 mouseDiff, glm::vec3 moveInput, float dt);
	void UpdateFly(const class Input& input);
	void PrintPosition();
	glm::vec3 Forward() const;
	glm::vec3 Right() const;
	void SetMoveSpeed(float moveSpeed);
	float MoveSpeed() const;

private:
	void SyncFlyOrientationFromFacing();

	float m_lastFlyTime = 0.0f;
	float m_fieldOfView = 45.0f;
	float m_nearPlane = 0.1f;
	float m_farPlane = 1000000.0f;
	glm::mat4 m_projection_matrix{ 1.0f };
	glm::mat4 m_view_matrix{ 1.0f };
	glm::vec3 m_position{ 0.0f };
	glm::vec3 m_front{ 0.0f, 0.0f, 1.0f };
	glm::vec3 m_up{ 0.0f, 1.0f, 0.0f };
	glm::vec3 m_right{ 1.0f, 0.0f, 0.0f };
	GLFWwindow* m_window = nullptr;
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;
	float m_mouseSensitivity = 0.08f;
	float m_moveSpeed = 300.0f;
};

