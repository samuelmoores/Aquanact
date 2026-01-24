#include "Line.h"
#include <glad/glad.h>


Line::Line()
{
}

Line::Line(glm::vec3 minBounds, glm::vec3 maxBounds) :m_vao(-1), m_vbo(-1)
{
	// 8 corners of the cube
	glm::vec3 v000(minBounds.x, minBounds.y, minBounds.z);
	glm::vec3 v100(maxBounds.x, minBounds.y, minBounds.z);
	glm::vec3 v010(minBounds.x, maxBounds.y, minBounds.z);
	glm::vec3 v110(maxBounds.x, maxBounds.y, minBounds.z);

	glm::vec3 v001(minBounds.x, minBounds.y, maxBounds.z);
	glm::vec3 v101(maxBounds.x, minBounds.y, maxBounds.z);
	glm::vec3 v011(minBounds.x, maxBounds.y, maxBounds.z);
	glm::vec3 v111(maxBounds.x, maxBounds.y, maxBounds.z);

	// 12 edges (each edge is two vertices)
	std::vector<glm::vec3> edgeVerts = {
		// Bottom face
		v000, v100, v100, v110, v110, v010, v010, v000,
		// Top face
		v001, v101, v101, v111, v111, v011, v011, v001,
		// Vertical edges
		v000, v001, v100, v101, v010, v011, v110, v111
	};

	// Build m_vertices with colors
	m_vertices.clear();
	for (auto& v : edgeVerts)
	{
		m_vertices.push_back({ v.x, v.y, v.z, 1.0f, 0.0f, 1.0f }); // magenta for example
	}

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

