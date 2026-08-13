#pragma once

#include "Engine/Core/Camera.h"
#include "GLFW/glfw3.h"

class GameCamera final : public Camera {
public:
	GameCamera() = default;
	~GameCamera() override = default;

	void startUp() override;
	void shutDown() override;
	glm::mat4 GetProjectionMatrix() const override;
	glm::mat4 GetViewMatrix() const override;
	glm::vec3 GetPosition() const override;
	glm::vec3 GetFacing() const override;

	void SetPose(const glm::vec3& position, const glm::vec3& facing);

private:
	void RebuildView();

	float m_fieldOfView = 45.0f;
	float m_nearPlane = 0.1f;
	float m_farPlane = 1000000.0f;
	GLFWwindow* m_window = nullptr;
	glm::mat4 m_viewMatrix{1.0f};
	glm::vec3 m_position{0.0f, 0.0f, -10.0f};
	glm::vec3 m_facing{0.0f, 0.0f, 1.0f};
};
