#include "Axis.h"

Axis::Axis(float axisLength) :m_vao(-1), m_vbo(-1)
{
	m_vertices = {
		// X axis (Red)
		{-axisLength, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // origin
		{axisLength, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // x-direction

		// Y axis (Green)
		{0.0f, -axisLength, 0.0f, 0.0f, 1.0f, 0.0f},  // origin
		{0.0f, axisLength, 0.0f, 0.0f, 1.0f, 0.0f},  // y-direction

		// Z axis (Blue)
		{0.0f, 0.0f, -axisLength, 0.0f, 0.0f, 1.0f},  // origin
		{0.0f, 0.0f, axisLength, 0.0f, 0.0f, 1.0f},   // z-direction

	};

	for (int i = 0; i < axisLength * 2; i++)
	{
		//along x
		m_vertices.push_back({ i / 2.0f, 0.0f, axisLength, 1.0f, 1.0f, 1.0f });
		m_vertices.push_back({ i / 2.0f, 0.0f, -axisLength, 1.0f, 1.0f, 1.0f });

		m_vertices.push_back({ -i / 2.0f, 0.0f, axisLength, 1.0f, 1.0f, 1.0f });
		m_vertices.push_back({ -i / 2.0f, 0.0f, -axisLength, 1.0f, 1.0f, 1.0f });
	}

	for (int i = 0; i < axisLength * 2; i++)
	{
		//along z
		m_vertices.push_back({ axisLength, 0.0f, i / 2.0f, 1.0f, 1.0f, 1.0f });
		m_vertices.push_back({ -axisLength, 0.0f, i / 2.0f, 1.0f, 1.0f, 1.0f });

		m_vertices.push_back({ axisLength, 0.0f, -i / 2.0f, 1.0f, 1.0f, 1.0f });
		m_vertices.push_back({ -axisLength, 0.0f, -i / 2.0f, 1.0f, 1.0f, 1.0f });
	}

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
	m_shader.activate();
	m_shader.setUniform("projection", glm::perspective(glm::radians(45.0f), static_cast<float>(1200) / 800, 0.1f, 1000.0f));
}

void Axis::UpdateProjection(glm::mat4 projectionMatrix)
{
	m_shader.activate();
	m_shader.setUniform("projection", projectionMatrix);
}

void Axis::draw(glm::mat4 viewMatrix)
{
	m_shader.activate();
	m_shader.setUniform("view", viewMatrix);
	m_shader.setUniform("model", glm::mat4(1));
	glBindVertexArray(m_vao);
	glDrawArrays(GL_LINES, 0, m_vertices.size());
	glBindVertexArray(0);

}
