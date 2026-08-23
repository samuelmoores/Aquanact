#pragma once
#include <glm/glm.hpp>
#include "Engine/Core/Mesh.h"
#include "Engine/Core/ShaderProgram.h"

struct RenderCommand {
	Mesh* mesh;
	const ShaderProgram* shader;
	glm::mat4 modelMatrix;
	bool isSkinned;
	unsigned int entityId = 0;
	glm::vec3 worldBoundsMin{ 0.0f };
	glm::vec3 worldBoundsMax{ 0.0f };
	bool hasWorldBounds = false;
	// -1 draws every imported submesh; otherwise only this buffer is submitted.
	int subMeshIndex = -1;
};


