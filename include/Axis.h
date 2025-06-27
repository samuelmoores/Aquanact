#pragma once
#include <vector>
#include <glad/glad.h>
#include <ShaderProgram.h>

struct AxisVertex3D {
	float x, y, z;
	float r, g, b;
};

class Axis {
public:
	Axis();
	Axis(float axisLength);
	void UpdateProjection(glm::mat4 projectionMatrix);
	void draw(glm::mat4 viewMatrix);
private:
	std::vector<AxisVertex3D> m_vertices;
	uint32_t m_vao;
	uint32_t m_vbo;
	ShaderProgram m_shader;
};
