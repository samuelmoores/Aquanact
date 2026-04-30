#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "ShaderProgram.h"
#include "RenderCommand.h"
#include "Light.h"

class ShadowMap {
public:
	ShadowMap(int resolution = 1024);
	~ShadowMap();

	void ShadowPass(const std::vector<RenderCommand>& commands, const PointLight& light);
	void BindTexture(int unit) const;
	float FarPlane() const;

private:
	uint32_t m_fbo;
	uint32_t m_cubemap;
	int m_resolution;
	float m_farPlane;
	ShaderProgram m_shader;
};
