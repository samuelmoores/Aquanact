#include "Engine/Core/GameCamera.h"

#include "Engine/Core/Root.h"
#include "Engine/Core/Window.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <cmath>

void GameCamera::startUp()
{
	m_window = Root::Current().WindowRef().GLFW();
	RebuildView();
}

void GameCamera::shutDown()
{
	m_window = nullptr;
}

glm::mat4 GameCamera::GetProjectionMatrix() const
{
	int width = 1;
	int height = 1;
	if (m_window)
	{
		glfwGetWindowSize(m_window, &width, &height);
	}
	if (height <= 0) height = 1;
	return glm::perspective(glm::radians(m_fieldOfView),
		static_cast<float>(width) / static_cast<float>(height),
		m_nearPlane, m_farPlane);
}

glm::mat4 GameCamera::GetViewMatrix() const { return m_viewMatrix; }
glm::vec3 GameCamera::GetPosition() const { return m_position; }
glm::vec3 GameCamera::GetFacing() const { return m_facing; }

void GameCamera::SetPose(const glm::vec3& position, const glm::vec3& facing)
{
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return;
	m_position = position;
	if (std::isfinite(facing.x) && std::isfinite(facing.y) && std::isfinite(facing.z) &&
		glm::dot(facing, facing) > 1e-8f)
		m_facing = glm::normalize(facing);
	else
		m_facing = {0.0f, 0.0f, 1.0f};
	RebuildView();
}

void GameCamera::RebuildView()
{
	m_viewMatrix = glm::lookAt(m_position, m_position + m_facing, {0.0f, 1.0f, 0.0f});
}
