#pragma once

#include "Engine/Camera.h"
#include "Engine/EngineCamera.h"
#include "GLFW/glfw3.h"

class GameCamera final : public Camera {
public:
	GameCamera() = default;
	void startUp() override;
	void shutDown() override;
	glm::mat4 GetProjectionMatrix() const override;
	glm::mat4 GetViewMatrix() const override;
	glm::vec3 GetPosition() const override;
	glm::vec3 GetFacing() const override;
	void CopyFrom(const EngineCamera& camera);
	void SetPose(const glm::vec3& position, const glm::vec3& facing);

private:
	float m_fieldOfView = 45.0f;
	float m_nearPlane = 0.1f;
	float m_farPlane = 1000000.0f;
	glm::mat4 m_projection_matrix{ 1.0f };
	glm::mat4 m_view_matrix{ 1.0f };
	glm::vec3 m_position{ 0.0f, 0.0f, -10.0f };
	glm::vec3 m_front{ 0.0f, 0.0f, 1.0f };
	glm::vec3 m_up{ 0.0f, 1.0f, 0.0f };
	GLFWwindow* m_window = nullptr;
};

