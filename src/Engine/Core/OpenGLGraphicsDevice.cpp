#include "Engine/Core/OpenGLGraphicsDevice.h"

#include "Engine/Core/OpenGLGraphics.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/RenderCommand.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/ShaderProgram.h"
#include "Engine/Core/GLHeaders.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace {
	constexpr int DirectionalShadowMapResolution = 2048;
	constexpr int PointShadowMapResolution = 512;
	constexpr int DirectionalShadowTextureUnit = 3;
	constexpr int FirstPointShadowTextureUnit = 4;

	glm::mat4 AiToGlm(const aiMatrix4x4& aiMat)
	{
		glm::mat4 result;
		result[0][0] = aiMat.a1; result[1][0] = aiMat.a2; result[2][0] = aiMat.a3; result[3][0] = aiMat.a4;
		result[0][1] = aiMat.b1; result[1][1] = aiMat.b2; result[2][1] = aiMat.b3; result[3][1] = aiMat.b4;
		result[0][2] = aiMat.c1; result[1][2] = aiMat.c2; result[2][2] = aiMat.c3; result[3][2] = aiMat.c4;
		result[0][3] = aiMat.d1; result[1][3] = aiMat.d2; result[2][3] = aiMat.d3; result[3][3] = aiMat.d4;
		return result;
	}

	void UploadSkinning(const RenderCommand& command, const ShaderProgram* shader)
	{
		shader->setUniform("skinned", command.isSkinned);
		if (!command.isSkinned)
		{
			return;
		}

		const auto& assimpTransforms = command.mesh->GetSkeleton().finalTransformations;
		std::vector<glm::mat4> glmTransforms;
		glmTransforms.reserve(assimpTransforms.size());
		for (const aiMatrix4x4& aiMat : assimpTransforms)
		{
			glmTransforms.push_back(AiToGlm(aiMat));
		}
		shader->setUniform("finalBones", glmTransforms);
	}

	void DrawShadowCasters(const RenderCommand* commands, std::size_t commandCount, const ShaderProgram* shader)
	{
		for (std::size_t commandIndex = 0; commandIndex < commandCount; ++commandIndex)
		{
			const RenderCommand& command = commands[commandIndex];
			if (!command.mesh)
			{
				continue;
			}

			shader->setUniform("model", command.modelMatrix);
			UploadSkinning(command, shader);
			for (int bufferIndex = 0; bufferIndex < command.mesh->NumBuffers(); ++bufferIndex)
			{
				command.mesh->Bind(bufferIndex);
				glDrawElements(GL_TRIANGLES, command.mesh->FacesSize(bufferIndex), GL_UNSIGNED_INT,
					reinterpret_cast<void*>(static_cast<uintptr_t>(command.mesh->FacesOffset(bufferIndex) * sizeof(uint32_t))));
				command.mesh->UnBind();
			}
		}
	}

	bool SceneBounds(const RenderCommand* commands, std::size_t commandCount, glm::vec3& minBounds, glm::vec3& maxBounds)
	{
		minBounds = glm::vec3(std::numeric_limits<float>::max());
		maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
		bool foundBounds = false;
		for (std::size_t commandIndex = 0; commandIndex < commandCount; ++commandIndex)
		{
			const RenderCommand& command = commands[commandIndex];
			if (!command.mesh)
			{
				continue;
			}

			const glm::vec3 localMin = command.mesh->LocalMinBounds();
			const glm::vec3 localMax = command.mesh->LocalMaxBounds();
			for (int x = 0; x < 2; ++x)
			{
				for (int y = 0; y < 2; ++y)
				{
					for (int z = 0; z < 2; ++z)
					{
						const glm::vec3 localCorner(
							x ? localMax.x : localMin.x,
							y ? localMax.y : localMin.y,
							z ? localMax.z : localMin.z);
						const glm::vec3 worldCorner = glm::vec3(command.modelMatrix * glm::vec4(localCorner, 1.0f));
						if (!std::isfinite(worldCorner.x) || !std::isfinite(worldCorner.y) || !std::isfinite(worldCorner.z))
						{
							continue;
						}
						minBounds = glm::min(minBounds, worldCorner);
						maxBounds = glm::max(maxBounds, worldCorner);
						foundBounds = true;
					}
				}
			}
		}
		return foundBounds;
	}
}

void OpenGLGraphicsDevice::startUp(Window& window)
{
	if (m_initialized) {
		return;
	}

	m_window = &window;
	startUp();
}

void OpenGLGraphicsDevice::startUp()
{
	if (m_initialized || !m_window) {
		return;
	}

	if (!m_platform)
	{
		m_platform = std::make_unique<OpenGLGraphics>();
	}
	m_platform->startUp(*m_window);
	InitializeShadowMap();
	m_initialized = true;
}

void OpenGLGraphicsDevice::InitializeShadowMap()
{
	m_shadowShader = std::make_unique<ShaderProgram>();
	m_shadowShader->load("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
	m_pointShadowShader = std::make_unique<ShaderProgram>();
	m_pointShadowShader->load("shaders/point_shadow_depth.vert", "shaders/point_shadow_depth.frag");

	glGenFramebuffers(1, &m_shadowFramebuffer);
	glGenTextures(1, &m_shadowDepthTexture);
	glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, DirectionalShadowMapResolution, DirectionalShadowMapResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowDepthTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		ReleaseShadowMap();
		throw std::runtime_error("Failed to create directional shadow framebuffer");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &m_pointShadowFramebuffer);
	for (uint32_t& texture : m_pointShadowDepthTextures)
	{
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
		for (int face = 0; face < 6; ++face)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24,
				PointShadowMapResolution, PointShadowMapResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, m_pointShadowFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, texture, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			ReleaseShadowMap();
			throw std::runtime_error("Failed to create point-light shadow framebuffer");
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	m_pointShadowFarPlanes.fill(1.0f);
	m_pointShadowMapsReady.fill(false);
}

void OpenGLGraphicsDevice::ReleaseShadowMap()
{
	if (m_shadowDepthTexture != 0)
	{
		glDeleteTextures(1, &m_shadowDepthTexture);
		m_shadowDepthTexture = 0;
	}
	if (m_shadowFramebuffer != 0)
	{
		glDeleteFramebuffers(1, &m_shadowFramebuffer);
		m_shadowFramebuffer = 0;
	}
	for (uint32_t& texture : m_pointShadowDepthTextures)
	{
		if (texture != 0)
		{
			glDeleteTextures(1, &texture);
			texture = 0;
		}
	}
	if (m_pointShadowFramebuffer != 0)
	{
		glDeleteFramebuffers(1, &m_pointShadowFramebuffer);
		m_pointShadowFramebuffer = 0;
	}
	m_shadowShader.reset();
	m_pointShadowShader.reset();
	m_shadowMapReady = false;
	m_pointShadowMapsReady.fill(false);
	m_pointShadowFarPlanes.fill(1.0f);
}

void OpenGLGraphicsDevice::shutDown()
{
	ReleaseShadowMap();
	if (m_platform)
	{
		m_platform->shutDown();
		m_platform.reset();
	}

	m_window = nullptr;
	m_initialized = false;
}

OpenGLGraphicsDevice::~OpenGLGraphicsDevice()
{
	shutDown();
}

void OpenGLGraphicsDevice::BeginFrame()
{
	if (m_platform)
	{
		// Make the window's context current and update the viewport before any draw calls.
		m_platform->MakeCurrent();
		m_platform->UpdateViewport();
	}
}

void OpenGLGraphicsDevice::Clear(float r, float g, float b, float a)
{
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLGraphicsDevice::EndFrame()
{
	if (m_platform && m_platform->IsInitialized())
	{
		m_platform->SwapBuffers();
	}
}

void OpenGLGraphicsDevice::ConfigureDefaultState()
{
	if (m_platform)
	{
		// Used by the scene renderer and 3D content.
		m_platform->ConfigureDefaultState();
	}
}

void OpenGLGraphicsDevice::ConfigureGuiState()
{
	if (m_platform)
	{
		// Used by the GUI overlay after the scene pass has finished.
		m_platform->ConfigureGuiState();
	}
}

void OpenGLGraphicsDevice::RenderShadowMaps(const RenderCommand* commands, std::size_t commandCount, const LightingManager& lightingManager)
{
	m_shadowMapReady = false;
	m_pointShadowMapsReady.fill(false);
	if (!lightingManager.ShadowsEnabled() || !commands || commandCount == 0)
	{
		return;
	}

	bool renderedAnyShadowMap = false;
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);

	if (lightingManager.SunLight().castsShadows && m_shadowShader && m_shadowFramebuffer != 0)
	{
		glm::vec3 minBounds;
		glm::vec3 maxBounds;
		if (SceneBounds(commands, commandCount, minBounds, maxBounds))
		{
			const glm::vec3 sceneCenter = (minBounds + maxBounds) * 0.5f;
			const float sceneExtent = std::max(glm::length(maxBounds - minBounds) * 0.55f, 10.0f);
			glm::vec3 lightDirection = lightingManager.SunLight().direction;
			if (glm::length(lightDirection) <= 0.0001f)
			{
				lightDirection = glm::vec3(-0.3f, -1.0f, 0.2f);
			}
			lightDirection = glm::normalize(lightDirection);
			const glm::vec3 lightPosition = sceneCenter - lightDirection * (sceneExtent * 2.0f);
			const glm::vec3 lightUp = std::abs(glm::dot(lightDirection, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.98f
				? glm::vec3(0.0f, 0.0f, 1.0f)
				: glm::vec3(0.0f, 1.0f, 0.0f);
			const glm::mat4 lightView = glm::lookAt(lightPosition, sceneCenter, lightUp);
			const glm::mat4 lightProjection = glm::ortho(
				-sceneExtent, sceneExtent,
				-sceneExtent, sceneExtent,
				0.1f, sceneExtent * 4.0f);
			m_lightSpaceMatrix = lightProjection * lightView;

			glViewport(0, 0, DirectionalShadowMapResolution, DirectionalShadowMapResolution);
			glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
			glClear(GL_DEPTH_BUFFER_BIT);
			m_shadowShader->activate();
			m_shadowShader->setUniform("lightSpaceMatrix", m_lightSpaceMatrix);
			DrawShadowCasters(commands, commandCount, m_shadowShader.get());
			m_shadowMapReady = true;
			renderedAnyShadowMap = true;
		}
	}

	if (m_pointShadowShader && m_pointShadowFramebuffer != 0)
	{
		static const std::array<glm::vec3, 6> faceDirections = {
			glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)
		};
		static const std::array<glm::vec3, 6> faceUpDirections = {
			glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
			glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)
		};

		const int pointLightCount = std::min(static_cast<int>(lightingManager.PointLights().size()), LightingManager::MaxPointLights);
		for (int lightIndex = 0; lightIndex < pointLightCount; ++lightIndex)
		{
			const PointLight& pointLight = lightingManager.PointLights()[lightIndex];
			if (!pointLight.castsShadows || pointLight.intensity <= 0.0f || m_pointShadowDepthTextures[lightIndex] == 0)
			{
				continue;
			}

			const float farPlane = std::max(pointLight.radius, 1.0f);
			const glm::mat4 shadowProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, farPlane);
			m_pointShadowFarPlanes[lightIndex] = farPlane;

			glViewport(0, 0, PointShadowMapResolution, PointShadowMapResolution);
			glBindFramebuffer(GL_FRAMEBUFFER, m_pointShadowFramebuffer);
			m_pointShadowShader->activate();
			m_pointShadowShader->setUniform("lightPosition", pointLight.position);
			m_pointShadowShader->setUniform("farPlane", farPlane);
			for (int face = 0; face < 6; ++face)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
					m_pointShadowDepthTextures[lightIndex], 0);
				glClear(GL_DEPTH_BUFFER_BIT);
				const glm::mat4 shadowView = glm::lookAt(
					pointLight.position,
					pointLight.position + faceDirections[face],
					faceUpDirections[face]);
				m_pointShadowShader->setUniform("shadowMatrix", shadowProjection * shadowView);
				DrawShadowCasters(commands, commandCount, m_pointShadowShader.get());
			}
			m_pointShadowMapsReady[lightIndex] = true;
			renderedAnyShadowMap = true;
		}
	}
	glDisable(GL_POLYGON_OFFSET_FILL);
	if (renderedAnyShadowMap)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (m_platform)
		{
			m_platform->UpdateViewport();
			m_platform->ConfigureDefaultState();
		}
	}
}

void OpenGLGraphicsDevice::Draw(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager)
{
	command.shader->activate();

	command.shader->setUniform("baseTexture", 0);
	command.shader->setUniform("specularTexture", 1);
	command.shader->setUniform("normalTexture", 2);
	command.shader->setUniform("shadowMap", DirectionalShadowTextureUnit);

	command.shader->setUniform("model", command.modelMatrix);
	command.shader->setUniform("view", camera.GetViewMatrix());
	command.shader->setUniform("projection", camera.GetProjectionMatrix());
	command.shader->setUniform("skinned", command.isSkinned);
	command.shader->setUniform("viewPos", camera.GetPosition());
	command.shader->setUniform("lightSpaceMatrix", m_lightSpaceMatrix);
	command.shader->setUniform("directionalShadowEnabled",
		lightingManager.ShadowsEnabled() && lightingManager.SunLight().castsShadows && m_shadowMapReady);
	for (int lightIndex = 0; lightIndex < LightingManager::MaxPointLights; ++lightIndex)
	{
		const std::string index = std::to_string(lightIndex);
		const bool lightCastsShadows = lightIndex < static_cast<int>(lightingManager.PointLights().size()) &&
			lightingManager.PointLights()[lightIndex].castsShadows;
		command.shader->setUniform("pointShadowMap" + index, FirstPointShadowTextureUnit + lightIndex);
		command.shader->setUniform("pointShadowReady[" + index + "]",
			lightingManager.ShadowsEnabled() && lightCastsShadows && m_pointShadowMapsReady[lightIndex]);
		command.shader->setUniform("pointShadowFarPlanes[" + index + "]", m_pointShadowFarPlanes[lightIndex]);
	}

	lightingManager.ApplyToShader(command.shader);

	UploadSkinning(command, command.shader);
	glActiveTexture(GL_TEXTURE0 + DirectionalShadowTextureUnit);
	glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
	for (int lightIndex = 0; lightIndex < LightingManager::MaxPointLights; ++lightIndex)
	{
		glActiveTexture(GL_TEXTURE0 + FirstPointShadowTextureUnit + lightIndex);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_pointShadowDepthTextures[lightIndex]);
	}

	int numBuffs = command.mesh->NumBuffers();
	for (int j = 0; j < numBuffs; j++) {
		const SubMeshMaterial& mat = command.mesh->GetMaterial(j);
		command.shader->setUniform("material", mat.phong);
		command.shader->setUniform("ambientColor", mat.ambientColor);
		command.shader->setUniform("hasBaseTexture", command.mesh->HasColorTexture(j));
		command.shader->setUniform("hasSpecularTexture", command.mesh->HasSpecularTexture(j));
		command.shader->setUniform("hasNormalTexture", command.mesh->HasNormalTexture(j));
		command.mesh->Bind(j);
		glDrawElements(GL_TRIANGLES, command.mesh->FacesSize(j), GL_UNSIGNED_INT, reinterpret_cast<void*>(static_cast<uintptr_t>(command.mesh->FacesOffset(j) * sizeof(uint32_t))));
		command.mesh->UnBind();
	}
	glActiveTexture(GL_TEXTURE0 + DirectionalShadowTextureUnit);
	glBindTexture(GL_TEXTURE_2D, 0);
	for (int lightIndex = 0; lightIndex < LightingManager::MaxPointLights; ++lightIndex)
	{
		glActiveTexture(GL_TEXTURE0 + FirstPointShadowTextureUnit + lightIndex);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}
	glActiveTexture(GL_TEXTURE0);
}


