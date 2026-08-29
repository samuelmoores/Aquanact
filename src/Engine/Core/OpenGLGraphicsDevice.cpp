#include "Engine/Core/OpenGLGraphicsDevice.h"

#include "Engine/Core/OpenGLGraphics.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/RenderCommand.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/ShaderProgram.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/Frustum.h"
#include "Engine/Core/OccluderSelector.h"

#include <algorithm>
#include <chrono>
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

	int FirstBuffer(const RenderCommand& command)
	{
		return command.subMeshIndex >= 0 ? command.subMeshIndex : 0;
	}

	int BufferEnd(const RenderCommand& command)
	{
		return command.subMeshIndex >= 0
			? std::min(command.subMeshIndex + 1, command.mesh->NumBuffers())
			: command.mesh->NumBuffers();
	}

	bool SubMeshVisible(const RenderCommand& command, int subMeshIndex, const Frustum& localFrustum)
	{
		return localFrustum.IntersectsAabb(
			command.mesh->SubMeshMinBounds(subMeshIndex),
			command.mesh->SubMeshMaxBounds(subMeshIndex));
	}

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

	void DrawShadowCasters(const RenderCommand* commands, std::size_t commandCount, const ShaderProgram* shader,
		const glm::mat4* lightViewProjection, std::size_t& drawCalls, std::uint64_t& triangles)
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
			const Frustum localLightFrustum = lightViewProjection
				? Frustum::FromViewProjection(*lightViewProjection * command.modelMatrix)
				: Frustum{};
			for (int bufferIndex = FirstBuffer(command); bufferIndex < BufferEnd(command); ++bufferIndex)
			{
				// Animated bind-pose bounds are not conservative, so only static
				// submeshes participate in shadow-frustum culling.
				if (lightViewProjection && !command.isSkinned &&
					!SubMeshVisible(command, bufferIndex, localLightFrustum))
				{
					continue;
				}
				command.mesh->Bind(bufferIndex);
				glDrawElements(GL_TRIANGLES, command.mesh->FacesSize(bufferIndex), GL_UNSIGNED_INT,
					reinterpret_cast<void*>(static_cast<uintptr_t>(command.mesh->FacesOffset(bufferIndex) * sizeof(uint32_t))));
				++drawCalls;
				triangles += static_cast<std::uint64_t>(command.mesh->FacesSize(bufferIndex)) / 3u;
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
	InitializeOcclusionQueries();
	m_initialized = true;
}

void OpenGLGraphicsDevice::InitializeOcclusionQueries()
{
	constexpr std::size_t QueryPoolSize = 128;
	m_occlusionBoundsShader = std::make_unique<ShaderProgram>();
	m_occlusionBoundsShader->load("shaders/occlusion_bounds.vert", "shaders/occlusion_bounds.frag");

	const float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
		-0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f
	};
	const std::uint16_t indices[] = {
		0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
		0, 4, 5, 0, 5, 1, 3, 2, 6, 3, 6, 7,
		0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2
	};

	glGenVertexArrays(1, &m_occlusionBoundsVao);
	glGenBuffers(1, &m_occlusionBoundsVbo);
	glGenBuffers(1, &m_occlusionBoundsEbo);
	glBindVertexArray(m_occlusionBoundsVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_occlusionBoundsVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_occlusionBoundsEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	m_occlusionQueryPool.resize(QueryPoolSize);
	std::vector<std::uint32_t> queryIds(QueryPoolSize);
	glGenQueries(static_cast<GLsizei>(queryIds.size()), queryIds.data());
	for (std::size_t index = 0; index < QueryPoolSize; ++index)
	{
		m_occlusionQueryPool[index].id = queryIds[index];
	}
}

void OpenGLGraphicsDevice::ReleaseOcclusionQueries()
{
	std::vector<std::uint32_t> queryIds;
	queryIds.reserve(m_occlusionQueryPool.size());
	for (const QuerySlot& slot : m_occlusionQueryPool)
	{
		if (slot.id != 0) queryIds.push_back(slot.id);
	}
	if (!queryIds.empty())
		glDeleteQueries(static_cast<GLsizei>(queryIds.size()), queryIds.data());
	m_occlusionQueryPool.clear();

	if (m_occlusionBoundsEbo != 0) glDeleteBuffers(1, &m_occlusionBoundsEbo);
	if (m_occlusionBoundsVbo != 0) glDeleteBuffers(1, &m_occlusionBoundsVbo);
	if (m_occlusionBoundsVao != 0) glDeleteVertexArrays(1, &m_occlusionBoundsVao);
	m_occlusionBoundsEbo = 0;
	m_occlusionBoundsVbo = 0;
	m_occlusionBoundsVao = 0;
	m_occlusionBoundsShader.reset();
}

void OpenGLGraphicsDevice::InitializeShadowMap()
{
	m_shadowShader = std::make_unique<ShaderProgram>();
	m_shadowShader->load("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
	m_pointShadowShader = std::make_unique<ShaderProgram>();
	m_pointShadowShader->load("shaders/point_shadow_depth.vert", "shaders/point_shadow_depth.frag");
#ifdef AQUANACT_EDITOR
	m_selectionOutlineShader = std::make_unique<ShaderProgram>();
	m_selectionOutlineShader->load("shaders/selection_outline.vert", "shaders/selection_outline.frag");
#endif

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
	m_selectionOutlineShader.reset();
	m_shadowMapReady = false;
	m_pointShadowMapsReady.fill(false);
	m_pointShadowFarPlanes.fill(1.0f);
}

void OpenGLGraphicsDevice::shutDown()
{
	ReleaseOcclusionQueries();
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
	m_frameStats = {};
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
	// Selection rendering temporarily sets the stencil write mask to zero.
	// Restore it before clearing so stale outlines cannot survive into the next
	// camera frame.
	glStencilMask(0xFF);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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
	const auto shadowPassStart = std::chrono::high_resolution_clock::now();
	m_shadowMapReady = false;
	m_pointShadowMapsReady.fill(false);
	if (!lightingManager.ShadowsEnabled() || !commands || commandCount == 0)
	{
		m_frameStats.shadowPassMs = 0.0;
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
			DrawShadowCasters(commands, commandCount, m_shadowShader.get(), &m_lightSpaceMatrix,
				m_frameStats.directionalShadowDrawCalls, m_frameStats.directionalShadowTriangles);
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
				const glm::mat4 shadowMatrix = shadowProjection * shadowView;
				m_pointShadowShader->setUniform("shadowMatrix", shadowMatrix);
				DrawShadowCasters(commands, commandCount, m_pointShadowShader.get(), &shadowMatrix,
					m_frameStats.pointShadowDrawCalls[lightIndex],
					m_frameStats.pointShadowTriangles[lightIndex]);
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
	m_frameStats.shadowDrawCalls = m_frameStats.directionalShadowDrawCalls;
	m_frameStats.shadowTriangles = m_frameStats.directionalShadowTriangles;
	for (int lightIndex = 0; lightIndex < LightingManager::MaxPointLights; ++lightIndex)
	{
		m_frameStats.shadowDrawCalls += m_frameStats.pointShadowDrawCalls[lightIndex];
		m_frameStats.shadowTriangles += m_frameStats.pointShadowTriangles[lightIndex];
	}
	m_frameStats.shadowPassMs = std::chrono::duration<double, std::milli>(
		std::chrono::high_resolution_clock::now() - shadowPassStart).count();
}

void OpenGLGraphicsDevice::RenderOccluderDepth(const RenderCommand* commands,
	std::size_t commandCount, const std::vector<SelectedOccluder>& occluders,
	const Camera& camera)
{
	if (!commands || occluders.empty() || !m_shadowShader)
	{
		return;
	}

	const auto passStart = std::chrono::high_resolution_clock::now();
	GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
	GLboolean depthMask = GL_TRUE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
	GLint depthFunction = GL_LESS;
	glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
	const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
	const GLboolean polygonOffsetEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
	GLfloat polygonOffsetFactor = 0.0f;
	GLfloat polygonOffsetUnits = 0.0f;
	glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor);
	glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDisable(GL_BLEND);
	// Push prepass depth slightly away from the camera so the normal GL_LESS
	// main pass can redraw the same surfaces and populate color normally.
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 1.0f);

	m_shadowShader->activate();
	// shadow_depth.vert names this camera transform lightSpaceMatrix; using a
	// nonexistent shadowMatrix leaves the uniform at its default and clips the
	// entire prepass, making every occlusion query report visible.
	m_shadowShader->setUniform("lightSpaceMatrix",
		camera.GetProjectionMatrix() * camera.GetViewMatrix());
	m_shadowShader->setUniform("skinned", false);

	for (const SelectedOccluder& selected : occluders)
	{
		const RenderCommand* command = nullptr;
		for (std::size_t commandIndex = 0; commandIndex < commandCount; ++commandIndex)
		{
			if (commands[commandIndex].entityId == selected.key.entityId)
			{
				command = &commands[commandIndex];
				break;
			}
		}
		if (!command || !command->mesh || command->isSkinned) continue;
		if (selected.key.subMeshIndex < 0 ||
			selected.key.subMeshIndex >= command->mesh->NumBuffers()) continue;

		m_shadowShader->setUniform("model", command->modelMatrix);
		command->mesh->Bind(selected.key.subMeshIndex);
		glDrawElements(GL_TRIANGLES, command->mesh->FacesSize(selected.key.subMeshIndex),
			GL_UNSIGNED_INT, reinterpret_cast<void*>(static_cast<uintptr_t>(
				command->mesh->FacesOffset(selected.key.subMeshIndex) * sizeof(uint32_t))));
		++m_frameStats.occluderPrepassDrawCalls;
		m_frameStats.occluderPrepassTriangles += static_cast<std::uint64_t>(
			command->mesh->FacesSize(selected.key.subMeshIndex)) / 3u;
		command->mesh->UnBind();
	}

	glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
	glDepthMask(depthMask);
	glDepthFunc(depthFunction);
	if (depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	glPolygonOffset(polygonOffsetFactor, polygonOffsetUnits);
	if (polygonOffsetEnabled) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL);
	glUseProgram(0);
	glBindVertexArray(0);
	m_frameStats.occluderPrepassMs = std::chrono::duration<double, std::milli>(
		std::chrono::high_resolution_clock::now() - passStart).count();
}

void OpenGLGraphicsDevice::PollOcclusionQueries(
	std::vector<OcclusionQueryResult>& results)
{
	const auto pollStart = std::chrono::high_resolution_clock::now();
	results.clear();
	for (QuerySlot& slot : m_occlusionQueryPool)
	{
		if (!slot.pending) continue;
		GLint available = GL_FALSE;
		glGetQueryObjectiv(slot.id, GL_QUERY_RESULT_AVAILABLE, &available);
		if (available == GL_FALSE) continue;

		GLuint samplesPassed = GL_TRUE;
		glGetQueryObjectuiv(slot.id, GL_QUERY_RESULT, &samplesPassed);
		results.push_back({ slot.nodeIndex, slot.generation, samplesPassed != GL_FALSE });
		++m_frameStats.occlusionQueryResults;
		if (samplesPassed != GL_FALSE)
			++m_frameStats.occlusionVisibleResults;
		else
			++m_frameStats.occlusionOccludedResults;
		slot.pending = false;
		slot.nodeIndex = OcclusionBvh::InvalidIndex;
	}

	m_frameStats.occlusionQueriesPending = 0;
	for (const QuerySlot& slot : m_occlusionQueryPool)
		if (slot.pending) ++m_frameStats.occlusionQueriesPending;
	m_frameStats.occlusionQueryPollMs = std::chrono::duration<double, std::milli>(
		std::chrono::high_resolution_clock::now() - pollStart).count();
}

void OpenGLGraphicsDevice::IssueOcclusionQueries(const OcclusionBvh& bvh,
	const std::vector<std::uint32_t>& nodeIndices, std::uint64_t generation,
	const Camera& camera)
{
	const auto issueStart = std::chrono::high_resolution_clock::now();
	if (!m_occlusionBoundsShader || m_occlusionBoundsVao == 0 || nodeIndices.empty())
		return;

	GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
	GLboolean depthMask = GL_TRUE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
	GLint depthFunction = GL_LESS;
	glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
	const GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
	const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	m_occlusionBoundsShader->activate();
	m_occlusionBoundsShader->setUniform("viewProjection",
		camera.GetProjectionMatrix() * camera.GetViewMatrix());
	glBindVertexArray(m_occlusionBoundsVao);

	const auto& nodes = bvh.Nodes();
	const Frustum cameraFrustum = Frustum::FromViewProjection(
		camera.GetProjectionMatrix() * camera.GetViewMatrix());
	for (const std::uint32_t nodeIndex : nodeIndices)
	{
		if (nodeIndex >= nodes.size()) continue;
		if (!cameraFrustum.IntersectsAabb(
			nodes[nodeIndex].boundsMin, nodes[nodeIndex].boundsMax)) continue;
		const glm::vec3 cameraPosition = camera.GetPosition();
		const glm::vec3 cameraMargin(0.05f);
		if (glm::all(glm::greaterThanEqual(cameraPosition,
			nodes[nodeIndex].boundsMin - cameraMargin)) &&
			glm::all(glm::lessThanEqual(cameraPosition,
				nodes[nodeIndex].boundsMax + cameraMargin)))
		{
			// A proxy surrounding the eye can be clipped completely even though
			// its contents are visible. Such a node must remain conservative.
			continue;
		}
		bool alreadyPending = false;
		for (const QuerySlot& slot : m_occlusionQueryPool)
		{
			if (slot.pending && slot.generation == generation && slot.nodeIndex == nodeIndex)
			{
				alreadyPending = true;
				break;
			}
		}
		if (alreadyPending) continue;

		QuerySlot* availableSlot = nullptr;
		for (QuerySlot& slot : m_occlusionQueryPool)
		{
			if (!slot.pending)
			{
				availableSlot = &slot;
				break;
			}
		}
		if (!availableSlot) break;

		const OcclusionBvhNode& node = nodes[nodeIndex];
		const glm::vec3 center = (node.boundsMin + node.boundsMax) * 0.5f;
		const glm::vec3 size = node.boundsMax - node.boundsMin;
		const glm::vec3 expansion = glm::max(size * 0.01f, glm::vec3(0.01f));
		const glm::mat4 model = glm::translate(glm::mat4(1.0f), center) *
			glm::scale(glm::mat4(1.0f), size + expansion * 2.0f);
		m_occlusionBoundsShader->setUniform("model", model);

		glBeginQuery(GL_ANY_SAMPLES_PASSED, availableSlot->id);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, nullptr);
		glEndQuery(GL_ANY_SAMPLES_PASSED);
		availableSlot->pending = true;
		availableSlot->nodeIndex = nodeIndex;
		availableSlot->generation = generation;
		++m_frameStats.occlusionQueriesIssued;
	}

	glBindVertexArray(0);
	glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
	glDepthMask(depthMask);
	glDepthFunc(depthFunction);
	if (depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	glUseProgram(0);

	m_frameStats.occlusionQueriesPending = 0;
	for (const QuerySlot& slot : m_occlusionQueryPool)
		if (slot.pending) ++m_frameStats.occlusionQueriesPending;
	m_frameStats.occlusionQueryIssueMs = std::chrono::duration<double, std::milli>(
		std::chrono::high_resolution_clock::now() - issueStart).count();
}

int OpenGLGraphicsDevice::ViewportWidth() const
{
	return m_platform ? m_platform->ViewportWidth() : 0;
}

int OpenGLGraphicsDevice::ViewportHeight() const
{
	return m_platform ? m_platform->ViewportHeight() : 0;
}

void OpenGLGraphicsDevice::Draw(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager)
{
	DrawInternal(command, camera, lightingManager, nullptr, nullptr, nullptr, nullptr, nullptr);
}

void OpenGLGraphicsDevice::DrawCulled(const RenderCommand& command, const Camera& camera,
	const LightingManager& lightingManager, const Frustum& frustum,
	std::size_t& visibleSubMeshes, std::size_t& culledSubMeshes,
	const std::vector<std::uint8_t>* occlusionVisibility,
	std::size_t* occlusionCulledSubMeshes)
{
	DrawInternal(command, camera, lightingManager, &frustum, &visibleSubMeshes,
		&culledSubMeshes, occlusionVisibility, occlusionCulledSubMeshes);
}

void OpenGLGraphicsDevice::DrawInternal(const RenderCommand& command, const Camera& camera,
	const LightingManager& lightingManager, const Frustum* frustum,
	std::size_t* visibleSubMeshes, std::size_t* culledSubMeshes,
	const std::vector<std::uint8_t>* occlusionVisibility,
	std::size_t* occlusionCulledSubMeshes)
{
	// Test the complete batch before doing any shader, lighting, or texture work.
	// This matters for grouped level entities whose aggregate AABB intersects the
	// frustum even though every individual imported mesh is outside it.
	if (frustum)
	{
		const auto visibilityStart = std::chrono::high_resolution_clock::now();
		const Frustum localFrustum = Frustum::FromViewProjection(
			camera.GetProjectionMatrix() * camera.GetViewMatrix() * command.modelMatrix);

		bool anyVisible = false;

		for (int bufferIndex = FirstBuffer(command); bufferIndex < BufferEnd(command); ++bufferIndex)
		{
			if (occlusionVisibility &&
				(static_cast<std::size_t>(bufferIndex) >= occlusionVisibility->size() ||
				(*occlusionVisibility)[static_cast<std::size_t>(bufferIndex)] == 0))
			{
				if (occlusionCulledSubMeshes) ++(*occlusionCulledSubMeshes);
				continue;
			}
			if (SubMeshVisible(command, bufferIndex, localFrustum))
			{
				anyVisible = true;
				if (visibleSubMeshes) ++(*visibleSubMeshes);
			}
			else if (culledSubMeshes)
			{
				++(*culledSubMeshes);
			}
		}
		if (!anyVisible)
		{
			m_frameStats.mainVisibilityTestMs += std::chrono::duration<double, std::milli>(
				std::chrono::high_resolution_clock::now() - visibilityStart).count();
			return;
		}
		m_frameStats.mainVisibilityTestMs += std::chrono::duration<double, std::milli>(
			std::chrono::high_resolution_clock::now() - visibilityStart).count();
	}
	const Frustum localFrustum = frustum
		? Frustum::FromViewProjection(camera.GetProjectionMatrix() * camera.GetViewMatrix() * command.modelMatrix)
		: Frustum{};

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

	for (int j = FirstBuffer(command); j < BufferEnd(command); ++j) 
	{
		if (occlusionVisibility &&
			(static_cast<std::size_t>(j) >= occlusionVisibility->size() ||
			(*occlusionVisibility)[static_cast<std::size_t>(j)] == 0))
		{
			continue;
		}
		if (frustum && !SubMeshVisible(command, j, localFrustum))
		{
			continue;
		}
		const SubMeshMaterial& mat = command.mesh->GetMaterial(j);
		command.shader->setUniform("material", mat.phong);
		command.shader->setUniform("ambientColor", mat.ambientColor);
		command.shader->setUniform("hasBaseTexture", command.mesh->HasColorTexture(j));
		command.shader->setUniform("hasSpecularTexture", command.mesh->HasSpecularTexture(j));
		command.shader->setUniform("hasNormalTexture", command.mesh->HasNormalTexture(j));
		command.mesh->Bind(j);

		glDrawElements(GL_TRIANGLES, command.mesh->FacesSize(j), GL_UNSIGNED_INT, reinterpret_cast<void*>(static_cast<uintptr_t>(command.mesh->FacesOffset(j) * sizeof(uint32_t))));
		++m_frameStats.mainDrawCalls;
		m_frameStats.mainTriangles += static_cast<std::uint64_t>(command.mesh->FacesSize(j)) / 3u;
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

void OpenGLGraphicsDevice::DrawSelected(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager)
{
	glEnable(GL_STENCIL_TEST);
	glStencilMask(0xFF);
	glStencilFunc(GL_ALWAYS, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	Draw(command, camera, lightingManager);
	glStencilMask(0x00);
	glDisable(GL_STENCIL_TEST);
}

void OpenGLGraphicsDevice::DrawSelectionOutline(const RenderCommand& command, const Camera& camera)
{
	if (!m_selectionOutlineShader || !command.mesh)
		return;

	GLboolean previousDepthMask = GL_TRUE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
	const GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
	const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
	const GLboolean stencilWasEnabled = glIsEnabled(GL_STENCIL_TEST);
	GLint previousDepthFunction = GL_LESS;
	GLint previousCullFace = GL_BACK;
	glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunction);
	glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFace);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);

	m_selectionOutlineShader->activate();
	m_selectionOutlineShader->setUniform("model", command.modelMatrix);
	m_selectionOutlineShader->setUniform("view", camera.GetViewMatrix());
	m_selectionOutlineShader->setUniform("projection", camera.GetProjectionMatrix());
	m_selectionOutlineShader->setUniform("skinned", command.isSkinned);
	m_selectionOutlineShader->setUniform("viewportSize", glm::vec2(
		static_cast<float>(m_platform ? m_platform->ViewportWidth() : 1),
		static_cast<float>(m_platform ? m_platform->ViewportHeight() : 1)));
	UploadSkinning(command, m_selectionOutlineShader.get());
	const auto drawSelectedMesh = [&command]()
	{
		for (int bufferIndex = FirstBuffer(command); bufferIndex < BufferEnd(command); ++bufferIndex)
		{
			command.mesh->Bind(bufferIndex);
			glDrawElements(GL_TRIANGLES, command.mesh->FacesSize(bufferIndex), GL_UNSIGNED_INT,
				reinterpret_cast<void*>(static_cast<uintptr_t>(command.mesh->FacesOffset(bufferIndex) * sizeof(uint32_t))));
			command.mesh->UnBind();
		}
	};

	// A silhouette alone is invisible when a large floor extends past every
	// viewport edge. Add a light transparent tint over visible selected surfaces
	// so selection remains obvious without obscuring materials.
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	m_selectionOutlineShader->setUniform("outlineWidth", 0.0f);
	m_selectionOutlineShader->setUniform("outlineColor", glm::vec4(1.0f, 0.72f, 0.05f, 0.16f));
	drawSelectedMesh();

	// Draw the fixed-pixel silhouette border outside the selected stencil mask.
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_STENCIL_TEST);
	glStencilMask(0x00);
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	m_selectionOutlineShader->setUniform("outlineWidth", 3.0f);
	m_selectionOutlineShader->setUniform("outlineColor", glm::vec4(1.0f, 0.78f, 0.05f, 1.0f));
	drawSelectedMesh();

	glStencilMask(0xFF);
	glCullFace(previousCullFace);
	glDepthFunc(previousDepthFunction);
	glDepthMask(previousDepthMask);
	if (depthTestWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (stencilWasEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
}


