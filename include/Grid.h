#pragma once

#include <vector>
#include <glm/mat4x4.hpp>

class Line;

class Grid {
public:
	Grid(float w, float spacing);
	~Grid();

	void UpdateProjection(glm::mat4 projectionMatrix);
	void draw(glm::mat4 viewMatrix);

private:
	std::vector<Line*> m_lines;
};
