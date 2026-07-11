#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <Camera.h>
#include "Globals.h"
#include <Window.h>
#include <Object3D.h>


std::vector<Object3D*> objects_camera;

float yaw = 0.0f;
float pitch = 0.0f;
float sensitivity = 0.08f;

Camera::Camera()
{

	int width, height;
	glfwGetWindowSize(gWindow->GLFW(), &width, &height);

	m_projection_matrix = glm::perspective(glm::radians(45.0), static_cast<double>(width) / height, 0.1, 1000000.0);

	m_position = glm::vec3(-368.918, 412.794, -555.261);
	m_front = glm::vec3(0, 0, 1);
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
	glfwGetWindowSize(gWindow->GLFW(), &width, &height);
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


float Camera::ComputeSafeCameraDistance(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDist)
{
	const float minDist = 1.0f;
	const float padding = 10.0f;
	float closestHit = maxDist;
	float allowedDist = maxDist;

	// First pass: blocking objects cap how far the camera can go
	for (int i = 1; i < objects_camera.size(); i++)
	{
		if (objects_camera[i]->IgnoreCameraCollision()) continue;

		Mesh* mesh = objects_camera[i]->GetMesh();
		glm::vec3 mn = mesh->minBounds();
		glm::vec3 mx = mesh->maxBounds();

		// lookAt inside a blocking AABB — camera must stay at minDist
		if (rayOrigin.x >= mn.x && rayOrigin.x <= mx.x &&
			rayOrigin.y >= mn.y && rayOrigin.y <= mx.y &&
			rayOrigin.z >= mn.z && rayOrigin.z <= mx.z)
		{
			return minDist;
		}

		float t = 0;
		if (mesh->RayHit(rayOrigin, rayDir, t) && t < closestHit)
		{
			closestHit = t;
			allowedDist = std::max(minDist, t - padding);
		}
	}

	float cameraDist = std::min(maxDist, allowedDist);

	// Second pass: pass-through objects — camera may not rest inside their AABB.
	// If cameraDist lands in [tNear, tFar], push behind the object (tFar + padding)
	// when there is room; otherwise pull in front (tNear - padding).
	for (int i = 1; i < objects_camera.size(); i++)
	{
		if (!objects_camera[i]->IgnoreCameraCollision()) continue;

		Mesh* mesh = objects_camera[i]->GetMesh();
		glm::vec3 mn = mesh->minBounds();
		glm::vec3 mx = mesh->maxBounds();
		glm::vec3 invDir = 1.0f / rayDir;

		glm::vec3 t0 = (mn - rayOrigin) * invDir;
		glm::vec3 t1 = (mx - rayOrigin) * invDir;
		glm::vec3 tminV = glm::min(t0, t1);
		glm::vec3 tmaxV = glm::max(t0, t1);
		float tNear = std::max({ tminV.x, tminV.y, tminV.z });
		float tFar  = std::min({ tmaxV.x, tmaxV.y, tmaxV.z });

		if (tNear > tFar || tFar < 0.0f) continue;
		tNear = std::max(tNear, 0.0f);

		if (cameraDist < tNear || cameraDist > tFar) continue; // not inside this AABB

		float behindDist = tFar + padding;
		if (behindDist <= allowedDist)
			cameraDist = behindDist;
		else
			cameraDist = std::max(minDist, tNear - padding);
	}

	return cameraDist;
}

void Camera::CameraControl(glm::vec2 mouseDiff)
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

	float cameraDist = ComputeSafeCameraDistance(rayOrigin, rayDir, m_defaultDistance);
	m_position = m_lookAt + rayDir * cameraDist;
	m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);

}

void Camera::CameraControl(float scroll)
{
	const float minDistance = 100.0f;
	const float maxDistance = 2000.0f;
	const float zoomSpeed = 0.2f;

	// Calculate new target distance based on current default distance
	float zoomFactor = 1.0f - (scroll * zoomSpeed);
	float newDistance = m_defaultDistance * zoomFactor;

	// Clamp the distance
	newDistance = std::clamp(newDistance, minDistance, maxDistance);

	// Update the default distance (the target distance the camera wants to be at)
	m_defaultDistance = newDistance;

	// Update position immediately, respecting collision
	glm::vec3 dir = m_position - m_lookAt;
	if (glm::length(dir) > 0.001f)
	{
		glm::vec3 rayDir = glm::normalize(dir);
		float safeDist = ComputeSafeCameraDistance(m_lookAt, rayDir, m_defaultDistance);
		m_position = m_lookAt + rayDir * safeDist;
		m_view_matrix = glm::lookAt(m_position, m_lookAt, m_up);
	}
}

void Camera::Focus(glm::vec3 min, glm::vec3 max)
{
	m_lookAt = (min + max) * 0.5f;
	m_lookAt.y = max.y * 0.8f; // Look at upper body

	// Reset yaw/pitch to a default view
	yaw = 180.0f;
	pitch = -10.0f;

	float yawRad = glm::radians(yaw);
	float pitchRad = glm::radians(-pitch);

	glm::vec3 direction;
	direction.x = cos(pitchRad) * cos(yawRad);
	direction.y = sin(pitchRad);
	direction.z = cos(pitchRad) * sin(yawRad);
	direction = glm::normalize(direction);

	m_defaultDistance = 500.0f; // Default zoom distance
	m_position = m_lookAt - direction * m_defaultDistance;
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



glm::vec3 Camera::Forward()
{
	glm::vec3 cameraLookDir = glm::normalize(m_lookAt - m_position);
	cameraLookDir.y = 0.0f;
	return cameraLookDir;
}

glm::vec3 Camera::Right()
{
	glm::vec3 cameraLookDir = glm::normalize(m_lookAt - m_position);
	cameraLookDir.y = 0.0f;
	return glm::normalize(glm::cross(cameraLookDir, glm::vec3(0, 1, 0)));
}
