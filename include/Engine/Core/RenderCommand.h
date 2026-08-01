#pragma once
#include <glm/glm.hpp>
#include "Engine/Core/Mesh.h"
#include "Engine/Core/ShaderProgram.h"

struct RenderCommand {
	Mesh* mesh;
	const ShaderProgram* shader;
	glm::mat4 modelMatrix;
	bool isSkinned;
};


