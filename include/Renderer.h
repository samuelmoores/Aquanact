#pragma once
#include <glad/glad.h>
#include "Mesh.h"
#include "Camera.h"

struct RenderCommand {
	const Mesh* mesh;
	const ShaderProgram* shader;
	glm::mat4 modelMatrix;
	bool isSkinned;
};

class Renderer {
public:
	void Submit(const RenderCommand& command);
	void Flush(Camera* camera);

private:
	std::vector<RenderCommand> commands;
};