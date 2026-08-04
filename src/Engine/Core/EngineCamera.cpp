#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

#include "Engine/Core/EngineCamera.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Input.h"

void EngineCamera::startUp()
{
	m_window = Root::Current().WindowRef().GLFW();

	int width = 1;
	int height = 1;
	glfwGetWindowSize(m_window, &width, &height);
	if (height <= 0)
		height = 1;

	m_projection_matrix = glm::perspective(glm::radians(m_fieldOfView), static_cast<float>(width) / static_cast<float>(height), m_nearPlane, m_farPlane);
	m_position = glm::vec3(200.0f, 300.0f, 450.0f);
	m_front = glm::normalize(glm::vec3(0.0f) - m_position);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_right = glm::normalize(glm::cross(m_front, m_up));
	m_view_matrix = glm::lookAt(m_position, m_position + m_front, m_up);
	SyncFlyOrientationFromFacing();
	m_lastFlyTime = static_cast<float>(glfwGetTime());
}

void EngineCamera::shutDown()
{
}

glm::mat4 EngineCamera::GetProjectionMatrix() const
{
	int width = 1;
	int height = 1;
	glfwGetWindowSize(m_window, &width, &height);
	if (height <= 0)
		height = 1;

	return glm::perspective(glm::radians(m_fieldOfView), static_cast<float>(width) / static_cast<float>(height), m_nearPlane, m_farPlane);
}

glm::mat4 EngineCamera::GetViewMatrix() const
{
	return m_view_matrix;
}

glm::vec3 EngineCamera::GetPosition() const
{
	return m_position;
}

glm::vec3 EngineCamera::GetFacing() const
{
	return m_front;
}

void EngineCamera::PrintPosition()
{
	Root::Current().Debugger().LogMessage(
		"Camera position: " + std::to_string(m_position.x) + ", " +
		std::to_string(m_position.y) + ", " +
		std::to_string(m_position.z));
}

void EngineCamera::FlyControl(glm::vec2 mouseDiff, glm::vec3 moveInput, float dt)
{
	m_yaw += mouseDiff.x * m_mouseSensitivity;
	m_pitch += mouseDiff.y * m_mouseSensitivity;
	m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

	float yawRad = glm::radians(m_yaw);
	float pitchRad = glm::radians(m_pitch);

	glm::vec3 front;
	front.x = cos(pitchRad) * cos(yawRad);
	front.y = sin(pitchRad);
	front.z = cos(pitchRad) * sin(yawRad);
	m_front = glm::normalize(front);
	m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
	m_up = glm::normalize(glm::cross(m_right, m_front));

	if (glm::length(moveInput) > 0.0001f)
	{
		glm::vec3 movement = glm::normalize(moveInput);
		m_position += m_right * (movement.x * m_moveSpeed * dt);
		m_position += m_up * (movement.y * m_moveSpeed * dt);
		m_position += m_front * (movement.z * m_moveSpeed * dt);
	}

	m_view_matrix = glm::lookAt(m_position, m_position + m_front, m_up);
}

void EngineCamera::UpdateFly(const Input& input)
{
	const float dt = input.DeltaTime();

	if (input.LookBecameActive())
	{
		SyncFlyOrientationFromFacing();
		return;
	}

	if (input.LookActive() || glm::length(input.MoveInput()) > 0.0f)
	{
		FlyControl(input.MouseDelta(), input.MoveInput(), dt);
	}
}

void EngineCamera::SyncFlyOrientationFromFacing()
{
	glm::vec3 facing = glm::normalize(m_front);
	m_yaw = glm::degrees(atan2f(facing.z, facing.x));
	m_pitch = glm::degrees(asinf(glm::clamp(facing.y, -1.0f, 1.0f)));
	m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
	m_up = glm::normalize(glm::cross(m_right, m_front));
	m_view_matrix = glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::vec3 EngineCamera::Forward() const
{
	return m_front;
}

glm::vec3 EngineCamera::Right() const
{
	return m_right;
}

void EngineCamera::SetMoveSpeed(float moveSpeed)
{
	if (moveSpeed < 0.0f)
	{
		moveSpeed = 0.0f;
	}

	m_moveSpeed = moveSpeed;
}

float EngineCamera::MoveSpeed() const
{
	return m_moveSpeed;
}


