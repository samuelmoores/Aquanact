#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "Engine.h"

Camera::Camera()
{
	int width, height;
	glfwGetWindowSize(Engine::Window->GLFW(), &width, &height);

	m_projection_matrix = glm::perspective(glm::radians(45.0), static_cast<double>(width) / height, 0.1, 1000000.0);

	m_position = glm::vec3(463.462, 176.395, 417.303);
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

void Camera::CameraControl(glm::vec2 mouseDiff)
{
	float x = m_position.x;
	float z = m_position.z;
	float angle = glm::radians(90.0f) * Engine::DeltaFrameTime() * 1000.0f;

	if (mouseDiff.x > 0.0f)
		angle = angle;
	else if (mouseDiff.x < 0.0f)
		angle = -angle;
	else
		return;

	float xRotated = x * cos(angle) - z * sin(angle);
	float zRotated = x * sin(angle) + z * cos(angle);

	//rotate
	m_position.x = xRotated;
	m_position.z = zRotated;

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
	m_position.y = (max.y + ((max.y / 2.0f)));
	m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);
}

void Camera::PrintPosition()
{
	std::cout << "position: " << m_position.x << ", " << m_position.y << ", " << m_position.z << std::endl;
}
