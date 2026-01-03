#include "Line.h"
#include <glad/glad.h>


Line::Line()
{
}

Line::Line(float length, glm::vec3 origin, glm::vec3 direction) :m_vao(-1), m_vbo(-1)
{
	m_vertices = {
		// X axis (Red)
		{origin.x, origin.y, origin.z, 1.0f, 0.0f, 1.0f},  // origin
		{direction.x * length, direction.y * length, direction.z * length, 1.0f, 0.0f, 1.0f},  // direction
	};

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);

	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(LineVertex3D), m_vertices.data(), GL_STATIC_DRAW);

	const int numPosFloats = 3;
	const int numColorFloats = 3;
	// Position attribute
	glVertexAttribPointer(0, numPosFloats, GL_FLOAT, GL_FALSE, sizeof(LineVertex3D), (void*)0);
	glEnableVertexAttribArray(0);

	// Color attribute
	glVertexAttribPointer(1, numColorFloats, GL_FLOAT, GL_FALSE, sizeof(LineVertex3D), (void*)12);
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	m_shader.load("shaders/vertexColor.vert", "shaders/vertexColor.frag");
	m_shader.activate();
	m_shader.setUniform("projection", glm::perspective(glm::radians(45.0f), static_cast<float>(1200) / 800, 0.1f, 1000.0f));
}

void Line::UpdateProjection(glm::mat4 projectionMatrix)
{
	m_shader.activate();
	m_shader.setUniform("projection", projectionMatrix);
}

void Line::draw(glm::mat4 viewMatrix)
{
	m_shader.activate();
	m_shader.setUniform("view", viewMatrix);
	m_shader.setUniform("model", glm::mat4(1));
	glBindVertexArray(m_vao);
	glDrawArrays(GL_LINES, 0, m_vertices.size());
	glBindVertexArray(0);

}

