#pragma once
#include <vector>
#include <ShaderProgram.h>

struct LineVertex3D {
	float x, y, z;
	float r, g, b;
};

class Line {
public:
	Line();
	Line(glm::vec3 minBounds, glm::vec3 maxBounds);
	void UpdateProjection(glm::mat4 projectionMatrix);
	void draw(glm::mat4 viewMatrix);
private:
	std::vector<LineVertex3D> m_vertices;
	uint32_t m_vao;
	uint32_t m_vbo;
	ShaderProgram m_shader;
};
