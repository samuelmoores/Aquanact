#include "Engine/Core/OpenGLGraphicsDevice.h"

#include "Engine/Core/OpenGLGraphics.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/RenderCommand.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/ShaderProgram.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/Frustum.h"
#include "Engine/Core/StbImage.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Debug.h"

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
	constexpr int SplashTextureUnit = 5;
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
	m_particleShader = std::make_unique<ShaderProgram>();
	m_particleShader->load("shaders/particle.vert", "shaders/particle.frag");
	m_balatroShader = std::make_unique<ShaderProgram>();
	m_balatroShader->load("shaders/balatro.vert", "shaders/balatro.frag");
	glGenVertexArrays(1, &m_balatroVao);
	try
	{
		StbImage splashImage;
		splashImage.loadFromFile("spritesheets/rain splash.png");
		if (splashImage.getWidth() != 80 || splashImage.getHeight() != 16)
			throw std::runtime_error("expected an 80x16 sheet with five 16x16 frames");
		glGenTextures(1, &m_particleSplashTexture);
		glBindTexture(GL_TEXTURE_2D, m_particleSplashTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, splashImage.getWidth(), splashImage.getHeight(),
			0, GL_RGBA, GL_UNSIGNED_BYTE, splashImage.getData());
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	catch (const std::exception& exception)
	{
		m_particleSplashTexture = 0;
		Root::Current().Debugger().LogTagged(
			Debug::Severity::Warning, "ParticleTexture",
			std::string("Could not load splash sprite sheet: ") + exception.what());
	}
	glGenVertexArrays(1, &m_particleVao);
	glGenBuffers(1, &m_particleVbo);
	glBindVertexArray(m_particleVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), reinterpret_cast<void*>(offsetof(ParticleInstance, position)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), reinterpret_cast<void*>(offsetof(ParticleInstance, color)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), reinterpret_cast<void*>(offsetof(ParticleInstance, size)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), reinterpret_cast<void*>(offsetof(ParticleInstance, age)));
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), reinterpret_cast<void*>(offsetof(ParticleInstance, lifetime)));
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), reinterpret_cast<void*>(offsetof(ParticleInstance, shapeVariation)));
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	const uint32_t particleProgram = m_particleShader->GetID();
	m_particleModelUniform = glGetUniformLocation(particleProgram, "model");
	m_particleViewUniform = glGetUniformLocation(particleProgram, "view");
	m_particleProjectionUniform = glGetUniformLocation(particleProgram, "projection");
	m_particleViewportSizeUniform = glGetUniformLocation(particleProgram, "viewportSize");
	m_particleVisualShapeUniform = glGetUniformLocation(particleProgram, "particleVisualShape");
	m_particleSplashTextureUniform = glGetUniformLocation(particleProgram, "splashTexture");
	m_particleSplashBrightnessUniform = glGetUniformLocation(particleProgram, "splashBrightness");
	m_particleSplashOpacityUniform = glGetUniformLocation(particleProgram, "splashOpacity");
	if (m_particleSplashTextureUniform >= 0)
	{
		glUseProgram(particleProgram);
		glUniform1i(m_particleSplashTextureUniform, SplashTextureUnit);
		glUseProgram(0);
	}
	m_initialized = true;
}

void OpenGLGraphicsDevice::DrawMainMenuBackground()
{
	if (!m_balatroShader || !m_platform || m_platform->ViewportWidth() <= 0 || m_platform->ViewportHeight() <= 0)
		return;

	const GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
	const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
	const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
	GLboolean depthMask = GL_TRUE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glDepthMask(GL_FALSE);
	m_balatroShader->activate();
	static const auto startTime = std::chrono::steady_clock::now();
	const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
	m_balatroShader->setUniform("SPIN_ROTATION", -2.0f);
	m_balatroShader->setUniform("SPIN_SPEED", 7.0f);
	m_balatroShader->setUniform("OFFSET", glm::vec2(0.0f));
	m_balatroShader->setUniform("COLOUR_1", glm::vec4(0.871f, 0.267f, 0.231f, 1.0f));
	m_balatroShader->setUniform("COLOUR_2", glm::vec4(0.0f, 0.42f, 0.706f, 1.0f));
	m_balatroShader->setUniform("COLOUR_3", glm::vec4(0.086f, 0.137f, 0.145f, 1.0f));
	m_balatroShader->setUniform("CONTRAST", 3.5f);
	m_balatroShader->setUniform("LIGTHING", 0.4f);
	m_balatroShader->setUniform("SPIN_AMOUNT", 0.25f);
	m_balatroShader->setUniform("PIXEL_FILTER", 745.0f);
	m_balatroShader->setUniform("SPIN_EASE", 1.0f);
	m_balatroShader->setUniform("IS_ROTATE", false);
	m_balatroShader->setUniform("SHAPE_SCALE", 30.0f);
	m_balatroShader->setUniform("RADIAL_TWIST", 20.0f);
	m_balatroShader->setUniform("WARP_STRENGTH", 0.5f);
	m_balatroShader->setUniform("WARP_FREQUENCY", 1.0f);
	m_balatroShader->setUniform("WARP_ITERATIONS", 5);
	m_balatroShader->setUniform("BAND_WIDTH", 5.0f);
	m_balatroShader->setUniform("LIGHTING_THRESHOLD", 4.0f);
	m_balatroShader->setUniform("BACKGROUND_BLEND", 0.3f);
	m_balatroShader->setUniform("LOOP_TIME", 0.0f);
	m_balatroShader->ApplyEditorUniforms();
	// Engine-owned inputs must always be current and cannot be overridden by
	// editor controls or stale values from an earlier shader inspection.
	m_balatroShader->setUniform("iResolution", glm::vec3(
		static_cast<float>(m_platform->ViewportWidth()),
		static_cast<float>(m_platform->ViewportHeight()),
		1.0f));
	m_balatroShader->setUniform("iTime", elapsed);
	glBindVertexArray(m_balatroVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glUseProgram(0);

	glDepthMask(depthMask);
	if (depthTestWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (cullFaceWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (scissorWasEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

bool OpenGLGraphicsDevice::SetSplashTexture(const std::string& path)
{
	try
	{
		StbImage image;
		image.loadFromFile(path);
		if (image.getWidth() != 80 || image.getHeight() != 16)
			return false;

		uint32_t texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.getWidth(), image.getHeight(),
			0, GL_RGBA, GL_UNSIGNED_BYTE, image.getData());
		glBindTexture(GL_TEXTURE_2D, 0);
		if (m_particleSplashTexture != 0)
			glDeleteTextures(1, &m_particleSplashTexture);
		m_particleSplashTexture = texture;
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
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
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
	// Point-shadow PCF crosses cubemap faces. Seamless sampling prevents visible
	// face-edge curves where adjacent depth faces meet.
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
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
	m_particleShader.reset();
	m_balatroShader.reset();
	if (m_particleVbo != 0) glDeleteBuffers(1, &m_particleVbo);
	if (m_particleVao != 0) glDeleteVertexArrays(1, &m_particleVao);
	m_particleVbo = 0;
	m_particleVao = 0;
	m_particleModelUniform = -1;
	m_particleViewUniform = -1;
	m_particleProjectionUniform = -1;
	m_particleViewportSizeUniform = -1;
	m_particleVisualShapeUniform = -1;
	m_particleSplashTextureUniform = -1;
	m_particleSplashBrightnessUniform = -1;
	m_particleSplashOpacityUniform = -1;
	if (m_particleSplashTexture != 0) glDeleteTextures(1, &m_particleSplashTexture);
	m_particleSplashTexture = 0;
	if (m_balatroVao != 0) glDeleteVertexArrays(1, &m_balatroVao);
	m_balatroVao = 0;
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

void OpenGLGraphicsDevice::Draw(const RenderCommand& command, const Camera& camera, const LightingManager& lightingManager)
{
	DrawInternal(command, camera, lightingManager, nullptr, nullptr, nullptr);
}

void OpenGLGraphicsDevice::DrawParticles(const ParticleRenderCommand* commands, std::size_t commandCount,
	const Camera& camera, const LightingManager& lightingManager)
{
	if (!m_particleShader || !commands || commandCount == 0) return;

	std::size_t totalParticleCount = 0;
	for (std::size_t commandIndex = 0; commandIndex < commandCount; ++commandIndex)
	{
		if (commands[commandIndex].particles)
			totalParticleCount += commands[commandIndex].particles->size();
	}
	if (totalParticleCount == 0) return;

	m_particleShader->activate();
	const glm::mat4 view = camera.GetViewMatrix();
	const glm::mat4 projection = camera.GetProjectionMatrix();
	if (m_particleViewUniform >= 0)
		glUniformMatrix4fv(m_particleViewUniform, 1, GL_FALSE, &view[0][0]);
	if (m_particleProjectionUniform >= 0)
		glUniformMatrix4fv(m_particleProjectionUniform, 1, GL_FALSE, &projection[0][0]);
	if (m_particleViewportSizeUniform >= 0)
		glUniform2f(m_particleViewportSizeUniform,
			static_cast<float>(m_platform ? m_platform->ViewportWidth() : 1),
			static_cast<float>(m_platform ? m_platform->ViewportHeight() : 1));
	const DirectionalLight& sun = lightingManager.SunLight();
	m_particleShader->setUniform("sunLight.direction", sun.direction);
	m_particleShader->setUniform("sunLight.color", sun.color);
	m_particleShader->setUniform("sunLight.intensity", sun.intensity);
	m_particleShader->setUniform("sunLight.ambient", sun.ambient);
	const int pointLightCount = static_cast<int>(std::min<std::size_t>(
		lightingManager.PointLights().size(), LightingManager::MaxPointLights));
	m_particleShader->setUniform("pointLightCount", pointLightCount);
	for (int lightIndex = 0; lightIndex < pointLightCount; ++lightIndex)
	{
		const PointLight& pointLight = lightingManager.PointLights()[static_cast<std::size_t>(lightIndex)];
		const std::string prefix = "pointLights[" + std::to_string(lightIndex) + "].";
		m_particleShader->setUniform(prefix + "position", pointLight.position);
		m_particleShader->setUniform(prefix + "color", pointLight.color);
		m_particleShader->setUniform(prefix + "intensity", pointLight.intensity);
		m_particleShader->setUniform(prefix + "radius", pointLight.radius);
		m_particleShader->setUniform(prefix + "radiusFade", pointLight.radiusFade);
		m_particleShader->setUniform(prefix + "constant", pointLight.constant);
		m_particleShader->setUniform(prefix + "linear", pointLight.linear);
		m_particleShader->setUniform(prefix + "quadratic", pointLight.quadratic);
	}

	glBindVertexArray(m_particleVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
	glBufferData(GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(totalParticleCount * sizeof(ParticleInstance)),
		nullptr, GL_STREAM_DRAW);

	std::size_t particleOffset = 0;
	for (std::size_t commandIndex = 0; commandIndex < commandCount; ++commandIndex)
	{
		const ParticleRenderCommand& command = commands[commandIndex];
		if (!command.particles || command.particles->empty()) continue;
		const std::size_t uploadSize = command.particles->size() * sizeof(ParticleInstance);
		glBufferSubData(GL_ARRAY_BUFFER,
			static_cast<GLintptr>(particleOffset * sizeof(ParticleInstance)),
			static_cast<GLsizeiptr>(uploadSize), command.particles->data());
		particleOffset += command.particles->size();
	}

	const GLboolean depthMaskWasEnabled = [] { GLboolean value = GL_TRUE; glGetBooleanv(GL_DEPTH_WRITEMASK, &value); return value; }();
	const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);
	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_POINT_SPRITE);
	particleOffset = 0;
	ParticleBlendMode activeBlendMode = static_cast<ParticleBlendMode>(-1);
	for (std::size_t commandIndex = 0; commandIndex < commandCount; ++commandIndex)
	{
		const ParticleRenderCommand& command = commands[commandIndex];
		if (!command.particles || command.particles->empty()) continue;
		if (activeBlendMode != command.blendMode)
		{
			activeBlendMode = command.blendMode;
			if (activeBlendMode == ParticleBlendMode::Alpha)
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			else
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}
		if (m_particleModelUniform >= 0)
			glUniformMatrix4fv(m_particleModelUniform, 1, GL_FALSE, &command.modelMatrix[0][0]);
		if (m_particleSplashBrightnessUniform >= 0)
			glUniform1f(m_particleSplashBrightnessUniform, command.splashBrightness);
		if (m_particleSplashOpacityUniform >= 0)
			glUniform1f(m_particleSplashOpacityUniform, command.splashOpacity);
		if (m_particleVisualShapeUniform >= 0)
		{
			const bool useSplashSprite = command.visualShape == ParticleVisualShape::SplashSpriteSheet &&
				m_particleSplashTexture != 0;
			glUniform1i(m_particleVisualShapeUniform, useSplashSprite
				? static_cast<int>(command.visualShape)
				: static_cast<int>(command.visualShape == ParticleVisualShape::SplashSpriteSheet
					? ParticleVisualShape::SoftCircle : command.visualShape));
			if (useSplashSprite)
			{
				glActiveTexture(GL_TEXTURE0 + SplashTextureUnit);
				glBindTexture(GL_TEXTURE_2D, m_particleSplashTexture);
			}
		}
		glDrawArrays(GL_POINTS, static_cast<GLint>(particleOffset), static_cast<GLsizei>(command.particles->size()));
		particleOffset += command.particles->size();
		++m_frameStats.mainDrawCalls;
	}
	glDisable(GL_POINT_SPRITE);
	glDisable(GL_PROGRAM_POINT_SIZE);
	glDepthMask(depthMaskWasEnabled);
	if (!blendWasEnabled) glDisable(GL_BLEND);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}

void OpenGLGraphicsDevice::DrawCulled(const RenderCommand& command, const Camera& camera,
	const LightingManager& lightingManager, const Frustum& frustum,
	std::size_t& visibleSubMeshes, std::size_t& culledSubMeshes)
{
	DrawInternal(command, camera, lightingManager, &frustum, &visibleSubMeshes, &culledSubMeshes);
}

void OpenGLGraphicsDevice::DrawInternal(const RenderCommand& command, const Camera& camera,
	const LightingManager& lightingManager, const Frustum* frustum,
	std::size_t* visibleSubMeshes, std::size_t* culledSubMeshes)
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
	command.shader->ApplyEditorUniforms();

	command.shader->setUniform("baseTexture", 0);
	command.shader->setUniform("specularTexture", 1);
	command.shader->setUniform("normalTexture", 2);
	command.shader->setUniform("roughnessTexture", 3);
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
		if (frustum && !SubMeshVisible(command, j, localFrustum))
		{
			continue;
		}
		const SubMeshMaterial& mat = command.mesh->GetMaterial(j);
		command.shader->setUniform("material", mat.phong);
		command.shader->setUniform("ambientColor", mat.ambientColor);
		command.shader->setUniform("hasBaseTexture",
			command.mesh->HasColorTexture(j) && command.mesh->ColorTextureEnabled(j));
		command.shader->setUniform("hasSpecularTexture",
			command.mesh->HasSpecularTexture(j) && command.mesh->SpecularTextureEnabled(j));
		command.shader->setUniform("hasNormalTexture",
			command.mesh->HasNormalTexture(j) && command.mesh->NormalTextureEnabled(j));
		command.shader->setUniform("hasRoughnessTexture",
			command.mesh->HasRoughnessTexture(j) && command.mesh->RoughnessTextureEnabled(j));
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


