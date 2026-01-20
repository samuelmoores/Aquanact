#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "Engine.h"

Camera::Camera()
{
	int width, height;
	glfwGetWindowSize(Engine::Window->GLFW(), &width, &height);

	m_projection_matrix = glm::perspective(glm::radians(45.0), static_cast<double>(width) / height, 0.1, 1000000.0);

	m_position = glm::vec3(-9.87885, 0.0f, -417.303);
	m_front = glm::vec3(0);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_right = glm::normalize(glm::cross(m_front, m_up));
	m_view_matrix = glm::lookAt(m_position, m_front, m_up);
	m_lookAt = glm::vec3(0);
}

glm::mat4 Camera::GetProjectionMatrix()
{
	int width, height;
	glfwGetWindowSize(Engine::Window->GLFW(), &width, &height);
	m_projection_matrix = glm::perspective(glm::radians(45.0), static_cast<double>(width) / height, 0.1, 1000000.0);

	return m_projection_matrix;
}

glm::mat4 Camera::GetViewMatrix()
{
	return m_view_matrix;
}

glm::vec3 Camera::GetPosition()
{
	return m_position;
}

glm::vec3 Camera::GetFacing()
{
	return m_front;
}

float yaw = 0.0f;
float pitch = 0.0f;
float sensitivity = 0.08f;

void Camera::CameraControl(glm::vec2 mouseDiff)
{
	float floorY = 10.0f;
	float radius = glm::length(m_position - m_lookAt);

	// Clamp the argument to asin to avoid NaNs
	float minPitchRad = -asinf(glm::clamp(
		(m_lookAt.y - floorY) / radius,
		0.0f,
		1.0f
	));

	float minPitchDeg = glm::degrees(minPitchRad);

	yaw += mouseDiff.x * sensitivity;
	pitch += mouseDiff.y * sensitivity;

	pitch = std::clamp(pitch, minPitchDeg, 89.0f);

	pitch = std::clamp(pitch, -89.0f, 89.0f);

	float yawRad = glm::radians(yaw);
	float pitchRad = glm::radians(-pitch);

	glm::vec3 direction;
	direction.x = cos(pitchRad) * cos(yawRad);
	direction.y = sin(pitchRad);
	direction.z = cos(pitchRad) * sin(yawRad);

	direction = normalize(direction);

	glm::vec3 m_position = m_lookAt - direction * radius;
	m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);
}

void Camera::CameraControl(float scroll)
{
	if (scroll > 0)
		m_position /= (1.1f + Engine::DeltaFrameTime());
	else
		m_position *= (1.1f + Engine::DeltaFrameTime());

	m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);
}

void Camera::Focus(glm::vec3 min, glm::vec3 max)
{
	m_lookAt.y = (max.y - (min.y/2.0f)) / 2.0f;
	m_position.y = max.y;//(max.y + ((max.y / 2.0f)));
	m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);
}

void Camera::PrintPosition()
{
	std::cout << "position: " << m_position.x << ", " << m_position.y << ", " << m_position.z << std::endl;
}

void Camera::Move(glm::vec3 delta, glm::vec3 lookAt)
{
	m_position += delta;
	m_lookAt = lookAt;
	m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);

}
