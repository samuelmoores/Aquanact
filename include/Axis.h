#pragma once
#include <vector>
#include <ShaderProgram.h>

struct AxisVertex3D {
	float x, y, z;
	float r, g, b;
};

class Axis {
public:
	Axis();
	Axis(float axisLength);
	void draw(glm::mat4 viewMatrix);
	void UpdateProjection(glm::mat4 projectionMatrix);
private:
	std::vector<AxisVertex3D> m_vertices;
	uint32_t m_vao;
	uint32_t m_vbo;
	ShaderProgram m_shader;
};
