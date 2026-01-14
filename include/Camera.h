#pragma once
#include <iostream>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"


class Camera {
	public:
		Camera();
		glm::mat4 GetProjectionMatrix();
		glm::mat4 GetViewMatrix();
		glm::vec3 GetPosition();
		glm::vec3 GetFacing();
		void CameraControl(glm::vec2 mouseDiff, glm::vec3 playerPosition);
		void CameraControl(float scroll);
		void Focus(glm::vec3 min, glm::vec3 max);
		void PrintPosition();
		void Move(glm::vec3 delta, glm::vec3 lookAt);
	private:
		glm::mat4 m_projection_matrix;
		glm::mat4 m_view_matrix;
		glm::vec3 m_position;
		glm::vec3 m_rotation;
		glm::vec3 m_front;
		glm::vec3 m_up;
		glm::vec3 m_right;
		glm::vec3 m_lookAt;
		GLFWwindow* m_window;

};
