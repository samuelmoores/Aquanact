#include "Grid.h"

#include "Line.h"
#include <GLHeaders.h>
#include <cmath>

Grid::Grid(float w, float spacing)
{
	if (spacing <= 0.0f)
		spacing = 1.0f;

	const glm::vec3 color(0.45f, 0.45f, 0.45f);
	const glm::vec3 centerColor(0.75f, 0.75f, 0.75f);
	const float y = 0.0f;

	for (float x = -w; x <= w; x += spacing)
	{
		if (std::abs(x) < 0.0001f)
		{
			m_centerLines.push_back(new Line({
				{ x, y, -w, centerColor.x, centerColor.y, centerColor.z },
				{ x, y,  w, centerColor.x, centerColor.y, centerColor.z }
			}));
			continue;
		}

		m_lines.push_back(new Line({
			{ x, y, -w, color.x, color.y, color.z },
			{ x, y,  w, color.x, color.y, color.z }
		}));
	}

	for (float z = -w; z <= w; z += spacing)
	{
		if (std::abs(z) < 0.0001f)
		{
			m_centerLines.push_back(new Line({
				{ -w, y, z, centerColor.x, centerColor.y, centerColor.z },
				{  w, y, z, centerColor.x, centerColor.y, centerColor.z }
			}));
			continue;
		}

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
	for (Line* line : m_centerLines)
	{
		delete line;
	}
	m_centerLines.clear();
}

void Grid::UpdateProjection(glm::mat4 projectionMatrix)
{
	for (Line* line : m_lines)
	{
		line->UpdateProjection(projectionMatrix);
	}
	for (Line* line : m_centerLines)
	{
		line->UpdateProjection(projectionMatrix);
	}
}

void Grid::draw(glm::mat4 viewMatrix, bool drawCenterLines)
{
	for (Line* line : m_lines)
	{
		glLineWidth(1.0f);
		line->draw(viewMatrix);
	}

	if (drawCenterLines)
	{
		glLineWidth(4.0f);
		for (Line* line : m_centerLines)
		{
			line->draw(viewMatrix);
		}
	}
}
