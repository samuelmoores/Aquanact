#include "Grid.h"

#include "Line.h"
#include <cmath>

Grid::Grid(float w, float spacing)
{
	if (spacing <= 0.0f)
		spacing = 1.0f;

	const glm::vec3 color(0.45f, 0.45f, 0.45f);
	const float y = 0.0f;

	for (float x = -w; x <= w; x += spacing)
	{
		if (std::abs(x) < 0.0001f)
			continue;

		m_lines.push_back(new Line({
			{ x, y, -w, color.x, color.y, color.z },
			{ x, y,  w, color.x, color.y, color.z }
		}));
	}

	for (float z = -w; z <= w; z += spacing)
	{
		if (std::abs(z) < 0.0001f)
			continue;

		m_lines.push_back(new Line({
			{ -w, y, z, color.x, color.y, color.z },
			{  w, y, z, color.x, color.y, color.z }
		}));
	}
}

Grid::~Grid()
{
	for (Line* line : m_lines)
	{
		delete line;
	}
	m_lines.clear();
}

void Grid::UpdateProjection(glm::mat4 projectionMatrix)
{
	for (Line* line : m_lines)
	{
		line->UpdateProjection(projectionMatrix);
	}
}

void Grid::draw(glm::mat4 viewMatrix)
{
	for (Line* line : m_lines)
	{
		line->draw(viewMatrix);
	}
}
