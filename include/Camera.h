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
		void CameraControl(glm::vec2 mouseLast, float& diff);
	private:
		glm::mat4 m_projection_matrix;
		glm::mat4 m_view_matrix;
		glm::vec3 m_position;
		glm::vec3 m_rotation;
		glm::vec3 m_front;
		glm::vec3 m_up;
		glm::vec3 m_right;
		GLFWwindow* m_window;

};
