#include "Engine/GameCamera.h"

#include "Engine/EngineCamera.h"
#include "Engine/Root.h"
#include "Engine/Window.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

void GameCamera::startUp()
{
	m_window = Root::Current().WindowRef().GLFW();

	int width = 1;
	int height = 1;
	glfwGetWindowSize(m_window, &width, &height);
	if (height <= 0)
	{
		height = 1;
	}

	m_projection_matrix = glm::perspective(glm::radians(m_fieldOfView), static_cast<float>(width) / static_cast<float>(height), m_nearPlane, m_farPlane);
	m_position = glm::vec3(0.0f, 0.0f, -10.0f);
	m_front = glm::vec3(0.0f, 0.0f, 1.0f);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_view_matrix = glm::lookAt(m_position, glm::vec3(0.0f), m_up);
}

void GameCamera::shutDown()
{
}

glm::mat4 GameCamera::GetProjectionMatrix() const
{
	int width = 1;
	int height = 1;
	glfwGetWindowSize(m_window, &width, &height);
	if (height <= 0)
	{
		height = 1;
	}

	return glm::perspective(glm::radians(m_fieldOfView), static_cast<float>(width) / static_cast<float>(height), m_nearPlane, m_farPlane);
}

glm::mat4 GameCamera::GetViewMatrix() const
{
	return m_view_matrix;
}

glm::vec3 GameCamera::GetPosition() const
{
	return m_position;
}

glm::vec3 GameCamera::GetFacing() const
{
	return m_front;
}

void GameCamera::CopyFrom(const EngineCamera& camera)
{
	SetPose(camera.GetPosition(), camera.GetFacing());
	m_projection_matrix = camera.GetProjectionMatrix();
}

void GameCamera::SetPose(const glm::vec3& position, const glm::vec3& facing)
{
	m_position = position;
	m_front = glm::normalize(facing);
	if (glm::length(m_front) <= 0.0001f)
	{
		m_front = glm::vec3(0.0f, 0.0f, 1.0f);
	}
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_view_matrix = glm::lookAt(m_position, m_position + m_front, m_up);
}


