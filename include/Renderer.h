#pragma once
#include <GLHeaders.h>
#include <memory>
#include "Light.h"
#include "Mesh.h"
#include "Camera.h"
#include "Level.h"
#include "RenderCommand.h"
#include "ShadowMap.h"

class Renderer {
public:
	void Init();
	void Submit(const RenderCommand& command);
	void Flush(Camera* camera);
	void Loop();

	void AddPointLight(const PointLight& light);
	void ClearPointLights();

private:
	std::vector<RenderCommand> commands;
	std::vector<PointLight> m_pointLights;
	std::unique_ptr<ShadowMap> m_shadowMap;
};
