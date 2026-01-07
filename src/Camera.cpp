#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "Engine.h"

Camera::Camera()
{
	int width, height;
	glfwGetWindowSize(Engine::Window->GLFW(), &width, &height);

	m_projection_matrix = glm::perspective(glm::radians(45.0), static_cast<double>(width) / height, 0.1, 1000000.0);

	m_position = glm::vec3(5.0f, 2.0f, 5.0f);
	m_front = glm::vec3(-1.0f, -0.5f, -1.0f);
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
	float angle = glm::radians(3.0f);

	if (mouseDiff.x > 2.0f)
		angle = angle;
	else if (mouseDiff.x < -2.0f)
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
		m_position *= 1.1f;
	else
		m_position /= 1.1f;

	m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);
}

void Camera::Focus(glm::vec3 min, glm::vec3 max)
{
	std::cout << "max: " << min.x << ", " << min.y << ", " << min.z << std::endl;
	std::cout << "min: " << max.x << ", " << max.y << ", " << max.z << std::endl;

	glm::vec3 center = (min + max) / 2.0f;
	glm::vec3 size = max - min;
	float radius = glm::length(size) / 2.0f;

	float fovRadians = glm::radians(45.0f); // Convert degrees to radians
	float distance = radius / std::sin(fovRadians / 2.0f);

	std::cout << "distance: " << distance << std::endl;
	std::cout << "center: " << center.x << ", " << center.y << ", " << center.z << std::endl;
	//std::cout << "center of miximo object: -3.05176e-05, 119.131, 1.79399\n";

	// Position camera
	m_position = center + glm::vec3(max.x, center.y, distance/2);
	m_lookAt = center;


	//m_position = glm::vec3(164.494, 238.263, 275.224 + 100);
	//glm::vec3 lookAt(-3.05176e-05, 119.131, 1.79399);

	std::cout << "current position: " << m_position.x << ", " << m_position.y << ", " << m_position.z << std::endl;
	std::cout << "look at: " << center.x << ", " << center.y << ", " << center.z << std::endl;
	m_view_matrix = glm::lookAt(m_position, center, m_up);
}

void Camera::PrintPosition()
{
	std::cout << "position: " << m_position.x << ", " << m_position.y << ", " << m_position.z << std::endl;
}
