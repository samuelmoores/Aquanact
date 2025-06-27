#pragma once
#include <vector>
#include <glad/glad.h>
#include <ShaderProgram.h>

struct LineVertex3D {
	float x, y, z;
	float r, g, b;
};

class Line {
public:
	Line();
	Line(float length, glm::vec3 origin, glm::vec3 direction);
	void UpdateProjection(glm::mat4 projectionMatrix);
	void draw(glm::mat4 viewMatrix);
private:
	std::vector<LineVertex3D> m_vertices;
	uint32_t m_vao;
	uint32_t m_vbo;
	ShaderProgram m_shader;
};
