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
	int PointLightCount() const { return (int)m_pointLights.size(); }

	void SetFogColor(const glm::vec3& color) { m_fogColor = color; }
	void ActivateFog();

private:
	glm::vec3 m_fogColor = glm::vec3(1.0f, 1.0f, 1.0f);
	float m_fogBlend = 0.0f;
	bool m_fogActive = false;
	std::vector<RenderCommand> commands;
	std::vector<PointLight> m_pointLights;
	std::unique_ptr<ShadowMap> m_shadowMap;
};
