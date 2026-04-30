#pragma once
#include <glm/glm.hpp>
#include "Mesh.h"
#include "ShaderProgram.h"

struct RenderCommand {
	Mesh* mesh;
	const ShaderProgram* shader;
	glm::mat4 modelMatrix;
	bool isSkinned;
};
