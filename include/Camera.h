#pragma once
#include <iostream>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"


class Camera {
	public:
		Camera() = default;
		void startUp();
		void shutDown();
		glm::mat4 GetProjectionMatrix() const;
		glm::mat4 GetViewMatrix() const;
		glm::vec3 GetPosition() const;
		glm::vec3 GetFacing() const;
		void CameraControl(glm::vec2 mouseDiff);
		void CameraControl(float scroll);
		void Focus(glm::vec3 min, glm::vec3 max);
		void PrintPosition();
		void Move(glm::vec3 delta, glm::vec3 lookAt);

		glm::vec3 Forward() const;
		glm::vec3 Right() const;
	private:
		float ComputeSafeCameraDistance(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDist);
		glm::mat4 m_projection_matrix;
		glm::mat4 m_view_matrix;
		glm::vec3 m_position;
		glm::vec3 m_rotation;
		glm::vec3 m_front;
		glm::vec3 m_up;
		glm::vec3 m_right;
		glm::vec3 m_lookAt;
		GLFWwindow* m_window;
		float m_defaultDistance;
};
