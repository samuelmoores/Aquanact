#include "Engine/Axis.h"
#include <Engine/GLHeaders.h>

namespace {
	constexpr float GroundPlaneVisualOffset = 1.0f;
}

Axis::Axis(float axisLength) :m_vao(-1), m_vbo(-1)
{
	m_vertices = {
		// X axis (Red)
		{-axisLength, GroundPlaneVisualOffset, 0.0f, 1.0f, 0.0f, 0.0f},
		{ axisLength, GroundPlaneVisualOffset, 0.0f, 1.0f, 0.0f, 0.0f},

		// Y axis (Green)
		{0.0f, -axisLength, 0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f,  axisLength, 0.0f, 0.0f, 1.0f, 0.0f},

		// Z axis (Blue)
		{0.0f, GroundPlaneVisualOffset, -axisLength, 0.0f, 0.0f, 1.0f},
		{0.0f, GroundPlaneVisualOffset,  axisLength, 0.0f, 0.0f, 1.0f}
	};

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);

	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(AxisVertex3D), m_vertices.data(), GL_STATIC_DRAW);

	const int numPosFloats = 3;
	const int numColorFloats = 3;
	// Position attribute
	glVertexAttribPointer(0, numPosFloats, GL_FLOAT, GL_FALSE, sizeof(AxisVertex3D), (void*)0);
	glEnableVertexAttribArray(0);

	// Color attribute
	glVertexAttribPointer(1, numColorFloats, GL_FLOAT, GL_FALSE, sizeof(AxisVertex3D), (void*)12);
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	m_shader.load("shaders/vertexColor.vert", "shaders/vertexColor.frag");
}

void Axis::draw(glm::mat4 viewMatrix)
{
	m_shader.activate();
	m_shader.setUniform("view", viewMatrix);
	m_shader.setUniform("model", glm::mat4(1));
	glLineWidth(3.0f);
	glBindVertexArray(m_vao);
	glDrawArrays(GL_LINES, 0, m_vertices.size());
	glBindVertexArray(0);
}

void Axis::UpdateProjection(glm::mat4 projectionMatrix)
{
	m_shader.activate();
	m_shader.setUniform("projection", projectionMatrix);
}


