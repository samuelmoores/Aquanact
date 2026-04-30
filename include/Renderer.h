#pragma once
#include <glad/glad.h>
#include "Light.h"
#include "Mesh.h"
#include "Camera.h"
#include "Level.h"

struct RenderCommand {
	Mesh* mesh;
	const ShaderProgram* shader;
	glm::mat4 modelMatrix;
	bool isSkinned;
};

class Renderer {
public:
	void Submit(const RenderCommand& command);
	void Flush(Camera* camera);
	void Loop();

	void AddPointLight(const PointLight& light);
	void ClearPointLights();

private:
	std::vector<RenderCommand> commands;
	std::vector<PointLight> m_pointLights;
};