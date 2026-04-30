#include "ShadowMap.h"
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/matrix4x4.h>

static glm::mat4 AiToGlmShadow(const aiMatrix4x4& m)
{
	glm::mat4 r;
	r[0][0] = m.a1; r[1][0] = m.a2; r[2][0] = m.a3; r[3][0] = m.a4;
	r[0][1] = m.b1; r[1][1] = m.b2; r[2][1] = m.b3; r[3][1] = m.b4;
	r[0][2] = m.c1; r[1][2] = m.c2; r[2][2] = m.c3; r[3][2] = m.c4;
	r[0][3] = m.d1; r[1][3] = m.d2; r[2][3] = m.d3; r[3][3] = m.d4;
	return r;
}

ShadowMap::ShadowMap(int resolution)
	: m_resolution(resolution), m_farPlane(2000.0f), m_fbo(0), m_cubemap(0)
{
#ifndef __EMSCRIPTEN__
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

	glGenTextures(1, &m_cubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemap);
	for (int i = 0; i < 6; i++)
#ifdef __EMSCRIPTEN__
		// WebGL 2 / GLES 3 requires a sized internal format for depth textures
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT32F,
			resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
#else
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
			resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
#endif
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	// Attach face 0 as placeholder; ShadowPass updates per face
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_cubemap, 0);
#ifdef __EMSCRIPTEN__
	// WebGL 2 uses glDrawBuffers; tell it there is no color output
	GLenum drawBufs = GL_NONE;
	glDrawBuffers(1, &drawBufs);
#else
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
#endif
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifdef __EMSCRIPTEN__
	m_shader.load("shaders/web/shadow.vert", "shaders/web/shadow.frag");
#else
	m_shader.load("shaders/shadow.vert", "shaders/shadow.frag");
#endif
}

ShadowMap::~ShadowMap()
{
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteTextures(1, &m_cubemap);
}

void ShadowMap::ShadowPass(const std::vector<RenderCommand>& commands, const PointLight& light)
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, m_farPlane);
	glm::vec3 p = light.position;
	glm::mat4 shadowMatrices[6] = {
		proj * glm::lookAt(p, p + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
		proj * glm::lookAt(p, p + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
		proj * glm::lookAt(p, p + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
		proj * glm::lookAt(p, p + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
		proj * glm::lookAt(p, p + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
		proj * glm::lookAt(p, p + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
	};

	glViewport(0, 0, m_resolution, m_resolution);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	m_shader.activate();
	m_shader.setUniform("lightPos", light.position);
	m_shader.setUniform("farPlane", m_farPlane);

	for (int face = 0; face < 6; face++)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_cubemap, 0);
		glClear(GL_DEPTH_BUFFER_BIT);

		m_shader.setUniform("shadowMatrix", shadowMatrices[face]);

		for (const RenderCommand& cmd : commands)
		{
			m_shader.setUniform("model", cmd.modelMatrix);
			m_shader.setUniform("skinned", cmd.isSkinned);

			if (cmd.isSkinned)
			{
				const auto& transforms = cmd.mesh->GetSkeleton().finalTransformations;
				std::vector<glm::mat4> boneGlm;
				boneGlm.reserve(transforms.size());
				for (const aiMatrix4x4& m : transforms)
					boneGlm.push_back(AiToGlmShadow(m));
				m_shader.setUniform("finalBones", boneGlm);
			}

			int numBuffs = cmd.mesh->NumBuffers();
			for (int j = 0; j < numBuffs; j++)
			{
				cmd.mesh->Bind();
				glDrawElements(GL_TRIANGLES, cmd.mesh->FacesSize(), GL_UNSIGNED_INT, 0);
				cmd.mesh->UnBind();
			}
			cmd.mesh->ClearBufferIndex();
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void ShadowMap::BindTexture(int unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemap);
}

float ShadowMap::FarPlane() const
{
	return m_farPlane;
}
