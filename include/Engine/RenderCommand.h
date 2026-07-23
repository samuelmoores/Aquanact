#pragma once
#include <glm/glm.hpp>
#include "Engine/Mesh.h"
#include "Engine/ShaderProgram.h"

struct RenderCommand {
	Mesh* mesh;
	const ShaderProgram* shader;
	glm::mat4 modelMatrix;
	bool isSkinned;
};

