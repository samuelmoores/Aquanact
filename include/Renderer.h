#pragma once
#include "Mesh.h"

struct RenderCommand {
	const Mesh* mesh;
	const ShaderProgram* shader;
	glm::mat4 modelMatrix;
};

class Renderer {
public:
	void Submit(const RenderCommand& command);
	void Flush(Camera camera);

private:
	std::vector<RenderCommand> commands;
};