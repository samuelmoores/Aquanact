#pragma once
#include <iostream>
#include "SFML/Graphics.hpp"
#include "glm/glm.hpp"


class Camera {
	public:
		Camera(sf::Window& window);
		glm::mat4 GetProjectionMatrix();
		glm::mat4 GetViewMatrix();
		glm::vec3 GetPosition();
		glm::vec3 GetFacing();
		void CameraControl(sf::Vector2i& mouseLast, sf::Time& diff);
	private:
		glm::mat4 m_projection_matrix;
		glm::mat4 m_view_matrix;
		glm::vec3 m_position;
		glm::vec3 m_rotation;
		glm::vec3 m_front;
		glm::vec3 m_up;
		glm::vec3 m_right;
		sf::Window& m_window;

};
