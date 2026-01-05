#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "Engine.h"

Camera::Camera()
{
	int width, height;
	glfwGetWindowSize(Engine::Window->GLFW(), &width, &height);

	m_projection_matrix = glm::perspective(glm::radians(45.0), static_cast<double>(width) / height, 0.1, 100.0);

	m_position = glm::vec3(5.0f, 2.0f, 5.0f);
	m_front = glm::vec3(-1.0f, -0.5f, -1.0f);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_right = glm::normalize(glm::cross(m_front, m_up));
	m_view_matrix = glm::lookAt(m_position, m_front, m_up);
}

glm::mat4 Camera::GetProjectionMatrix()
{
	int width, height;
	glfwGetWindowSize(Engine::Window->GLFW(), &width, &height);
	m_projection_matrix = glm::perspective(glm::radians(45.0), static_cast<double>(width) / height, 0.1, 100.0);

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

void Camera::CameraControl(glm::vec2 mouseLast, float& diff)
{
	/*
	glm::vec2 mouseNow; //mouse pos
	glm::vec2 mouseDiff = mouseNow - mouseLast;
	float cameraSpeed = 3.0f * diff.asSeconds();

	if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
	{
		m_right = glm::normalize(glm::cross(m_front, m_up));

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
		{
			m_position += m_right * cameraSpeed;
		}
		else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
		{
			m_position += m_right * -cameraSpeed;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
		{
			m_position += m_front * cameraSpeed;
		}
		else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
		{
			m_position += m_front * -cameraSpeed;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E))
		{
			m_position += m_up * cameraSpeed;
		}
		else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q))
		{
			m_position += m_up * -cameraSpeed;
		}

		if (mouseDiff != sf::Vector2i(0, 0))
		{
			float sensitivity = 120.0f * diff.asSeconds();
			float thetaY = glm::radians(mouseDiff.x * sensitivity);
			float thetaX = glm::radians(mouseDiff.y * sensitivity);

			// Apply both yaw (around cameraUp) and pitch (around cameraRight)
			glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), -thetaY, m_up);    // yaw (left/right)
			rotation = glm::rotate(rotation, -thetaX, m_right);                  // pitch (up/down)

			m_front = glm::normalize(glm::vec3(rotation * glm::vec4(m_front, 0.0f)));
		}
	}


	mouseLast = mouseNow;

	*/
	m_view_matrix = glm::lookAt(m_position, m_position + m_front, m_up);
}

