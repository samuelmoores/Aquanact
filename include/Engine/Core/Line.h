#pragma once
#include <vector>
#include "Engine/Core/ShaderProgram.h"

struct LineVertex3D {
	float x, y, z;
	float r, g, b;
};

class Line {
public:
	Line();
	Line(glm::vec3 minBounds, glm::vec3 maxBounds);
	Line(std::vector<LineVertex3D> verts);
	~Line();
	void SetBounds(glm::vec3 minBounds, glm::vec3 maxBounds, const glm::vec3& color);
	void SetVertices(std::vector<LineVertex3D> verts);
	void SetColor(const glm::vec3& color);
	void UpdateProjection(glm::mat4 projectionMatrix);
	void draw(glm::mat4 viewMatrix);
	void draw(glm::mat4 viewMatrix, glm::mat4 modelMatrix);
private:
	std::vector<LineVertex3D> m_vertices;
	uint32_t m_vao;
	uint32_t m_vbo;
	ShaderProgram m_shader;
};


