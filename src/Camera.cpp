#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "Engine.h"

float yaw = 0.0f;
float pitch = 0.0f;
float sensitivity = 0.08f;

Camera::Camera()
{
	int width, height;
	glfwGetWindowSize(Engine::Window->GLFW(), &width, &height);

	m_projection_matrix = glm::perspective(glm::radians(45.0), static_cast<double>(width) / height, 0.1, 1000000.0);

	m_position = glm::vec3(0.405, 111.572f, -290.303);
	m_front = glm::vec3(0);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_right = glm::normalize(glm::cross(m_front, m_up));
	m_view_matrix = glm::lookAt(m_position, m_front, m_up);
	m_lookAt = glm::vec3(0);

	glm::vec3 dir = glm::normalize(m_lookAt - m_position);

	// Pitch: arcsin of Y
	pitch = -glm::degrees(asinf(dir.y));

	// Yaw: atan2(Z, X)
	yaw = glm::degrees(atan2f(dir.z, dir.x));

	m_defaultDistance = glm::length(m_position - m_lookAt);

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


void Camera::CameraControl(glm::vec2 mouseDiff, Mesh* mesh)
{
	glm::vec3 originalPosition = m_position;
	float floorY = 10.0f;
	float radius = glm::length(m_position - m_lookAt);

	// Clamp the argument to asin to avoid NaNs
	// Constrain pitch, not position, allowing camera to still yaw
	float minPitchRad = -asinf(glm::clamp((m_lookAt.y - floorY) / radius, 0.0f, 1.0f));

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

	glm::vec3 desiredPos = m_lookAt - direction * radius;

	//construct ray and max distance
	glm::vec3 rayOrigin = m_lookAt;
	glm::vec3 rayDir = glm::normalize(desiredPos - m_lookAt);
	float maxDist = m_defaultDistance;
	float allowedDistance = glm::length(desiredPos - m_lookAt);
	float minDist = 1.0f;
	float cameraDist = maxDist;
	float closestHit = maxDist;
	bool hit = false;

	float t = 0;
	float cameraPadding = 10.0f;
	if (Engine::Level->Objects()[1]->GetMesh()->RayHit(rayOrigin, rayDir, t) && t < closestHit)
	{
		allowedDistance = std::max(minDist, t - cameraPadding);
		if (t < minDist)
		{
			//problem
		}
	}
	else
	{
		allowedDistance = maxDist;
	}
	

	cameraDist = std::min(maxDist, allowedDistance);
	m_position = m_lookAt + rayDir * cameraDist;
	
	//m_position = desiredPos;
	m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);
	
}

void Camera::CameraControl(float scroll)
{
	glm::vec3 nextPosition = m_position;
	float minDistance = 60.0f;
	
	if (scroll > 0)
	{
		nextPosition /= (1.1f + Engine::DeltaFrameTime());
	}
	else
	{
		nextPosition *= (1.1f + Engine::DeltaFrameTime());
	}

	float nextDistanceFromPlayer = glm::length(nextPosition - m_lookAt);
	if (nextDistanceFromPlayer > minDistance)
	{
		m_position = nextPosition;
		m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);
	}

	m_defaultDistance = glm::length(m_position - m_lookAt);
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
}
